# include "bda/mutableBDASpace.inline.hpp"
# include "gc_implementation/shared/spaceDecorator.hpp"
# include "gc_implementation/parallelScavenge/parallelScavengeHeap.hpp"
# include "gc_implementation/parallelScavenge/psParallelCompact.hpp"
# include "memory/resourceArea.hpp"
# include "runtime/thread.hpp"
# include "runtime/vmThread.hpp"
# include "runtime/java.hpp"
# include "oops/oop.inline.hpp"
# include "oops/klassRegionMap.hpp"

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

// Forward declarations
class ParallelScavengeHeap;

// Definition of sizes - conformant with the PSParallelCompact class
const size_t MutableBDASpace::Log2MinRegionSize = 16; // 64K HeapWords
const size_t MutableBDASpace::MinRegionSize = (size_t)1 << Log2MinRegionSize;
const size_t MutableBDASpace::MinRegionSizeBytes = MinRegionSize << LogHeapWordSize;

const size_t MutableBDASpace::MinRegionSizeOffsetMask = MinRegionSize - 1;
const size_t MutableBDASpace::MinRegionAddrOffsetMask = MinRegionSizeBytes - 1;
const size_t MutableBDASpace::MinRegionAddrMask       = ~MinRegionAddrOffsetMask;

// Definition of minimum and aligned size of a container. Subject to change
// if it is too low. This values are conform to the ParallelOld (psParallelCompact.cpp).
const size_t MutableBDASpace::Log2BlockSize   = 7; // 128 words
const size_t MutableBDASpace::BlockSize       = (size_t)1 << Log2BlockSize;
const size_t MutableBDASpace::BlockSizeBytes  = BlockSize << LogHeapWordSize;

// Definition of sizes for the calculation of a collection size
int    MutableBDASpace::CGRPSpace::dnf = 0;
int    MutableBDASpace::CGRPSpace::delegation_level = 0;
int    MutableBDASpace::CGRPSpace::default_collection_size = 0;
int    MutableBDASpace::CGRPSpace::node_fields = 0;
size_t MutableBDASpace::CGRPSpace::segment_sz = 0;

// Initialization of the filler_header_array. Must be updated later.
size_t MutableBDASpace::_filler_header_size = 0;

// For offset allocation on the start array
ObjectStartArray * MutableBDASpace::_start_array = NULL;

//////////// ////////////////////////// //////////
//////////// MutableBDASpace::CGRPSpace //////////
//////////// ////////////////////////// //////////
container_t
MutableBDASpace::CGRPSpace::allocate_and_setup_container(HeapWord * start,
                                                         size_t reserved_sz, size_t obj_sz)
{
  container_t container = allocate_container();
  setup_container (container, MemRegion(start, reserved_sz), obj_sz);
  return container;
}

container_t
MutableBDASpace::CGRPSpace::allocate_container()
{
  // We allocate it with the general AllocateHeap since struct is not, by default,
  // subclass of one of the base VM classes (CHeap, ResourceObj, ... and friends).
  // It must be initialized prior to allocation because there's no default constructor.
  container_t container = (container_t)AllocateHeap(sizeof(struct container), mtGC);
  return container;
}

container_t
MutableBDASpace::CGRPSpace::allocate_large_container(size_t size)
{
  size_t reserved_sz = calculate_large_reserved_sz(size);
  container_t container = install_container_in_space(reserved_sz, size);
  // Here, the container pointer is installed on the RegionData object that manages
  // the address range this container spans during OldGC. This is for fast access
  // during summarize and update of the containers top pointers.
  if (container != NULL) {
    _manager->mark_container(container);
    _manager->allocate_block(container->_start);
    PSParallelCompact::install_container_in_region(container);
    if (BDAllocationVerboseLevel > 1) {
      print_allocation(container, true);
    }
  }
  
  return container;
}

container_t
MutableBDASpace::CGRPSpace::push_container(size_t size)
{
  container_t container;
  container_t first_seg;
  container_t last_seg;
  HeapWord * ptr;
  size_t reserved_sz = segment_sz;

  // Objects bigger than this size are, generally, large arrays.
  // Get the aligned reserved size, multiple of segment_sz.
  if (size > segment_sz) {
    reserved_sz = calculate_large_reserved_sz(size);
  }

  if (container_type() != KlassRegionMap::region_start_ptr()) {
    // Now the amount of space is reserved with a CAS and the resulting ptr
    // will be returned as the start of the collection struct.
    ptr = space()->cas_allocate(reserved_sz);
    
    // If there's no space left to allocate a new container, then
    // return NULL so that MutableBDASpace can handle the allocation
    // of a new container to the other_gen.
    if (ptr == NULL) {
      return (container_t)ptr;
    }
  
    // Get the container/segment in the RegionData that spans the addressable space
    container = PSParallelCompact::get_container_at_addr(ptr);
    last_seg  = PSParallelCompact::get_container_at_addr(ptr + reserved_sz);
    first_seg = container;

    // If there is a container/segment, then use this one, setting the limits accordingly
    if (container != NULL) {
      // Remove from the pool by decreasing the num of elements.
      _pool->phantom_remove();

      // This block follows these sequence of rules:
      //   a) if the thread grabbed a segment whose start is below ptr then it tries to grab
      //      the adjacent segment, still owned by the reserved space of the thread;
      //   b) if this condition is not satisfied it means that the thread may be grabbing
      //      a segment belonging to a different thread. Therefore, allocates a new one.
      if (container->_start != ptr) {
        container_t temp;
        temp = PSParallelCompact::get_container_at_addr(ptr + reserved_sz - 1);
        if (temp != container)
          container = temp;
        else
          container = allocate_container();
      }
  
      // Free unused segments in between and only those that are not going to be touched
      // by other threads.
      for (HeapWord * p = ptr + segment_sz; p < ptr + reserved_sz; p += segment_sz) {
        container_t temp;
        temp = PSParallelCompact::get_container_at_addr(p);
        if (temp != container && temp != first_seg && temp != last_seg) {
          // TODO: Is this really necessary? Aren't those containers/segments already
          // in the pool, thus freed?
          free_container(temp);
        }
      }

      // Setup the container last
      setup_container(container, MemRegion(ptr, reserved_sz), size);
    } else {
      container = allocate_and_setup_container(ptr, reserved_sz, size);
    }
  } else {
    // The amount of space is reserved with a CAS, but it must be aligned with the 512 byte blocks
    // on the card table.
    ptr = space()->cas_allocate_aligned(reserved_sz);
    
    // If there's no space left to allocate a new container, then
    // return NULL to trigger a FullGC.
    if (ptr == NULL) {
      return (container_t)ptr;
    }

    // Get a queued container from the pool or allocate a new one
    container = _pool->dequeue();
    if (container != NULL) {
      setup_container(container, MemRegion(ptr, reserved_sz), size);
    } else {
      container = allocate_and_setup_container(ptr, reserved_sz, size);
    }
  }

  // Add to the queue
  if (container != NULL) {
    assert (container->_next_segment == NULL, "should have been reset");
    assert (container->_prev_segment == NULL, "should have been reset");
    assert (container->_next == NULL, "should have been reset");
    assert (container->_previous == NULL, "should have been reset");
#ifdef ASSERT
    _segments_since_last_gc++;
#endif
    // MT safe, but only for subsequent enqueues/dequeues. If mixed, then the queue may break!
    // See gen_queue.hpp for more details.
    _containers->enqueue(container);
  }

  return container;
}

HeapWord *
MutableBDASpace::CGRPSpace::allocate_new_segment (size_t size, container_t& c)
{
  container_t container = push_container(size);
  container_t next,last; 
  
  if (container != NULL) {
    last = c; next = last->_next_segment;
    do {
      if (next == NULL && Atomic::cmpxchg_ptr(container,
                                              &(last->_next_segment),
                                              next) == next) {
        break;
      }
      last = last->_next_segment;
      next = last->_next_segment;
    } while (true);
    
    container->_prev_segment = last;
    // For the return val of the callee
    c = container;
    return container->_start;
  } else {    
    return NULL;
  }
}

void
MutableBDASpace::CGRPSpace::setup_container(container_t& container, MemRegion mr, size_t obj_sz)
{
  container->_start = mr.start(); container->_top = mr.start() + obj_sz;
  container->_hard_end = mr.end();
  container->_end = mr.end() - MutableBDASpace::_filler_header_size;
  container->_next_segment = NULL; container->_next = NULL; container->_prev_segment = NULL;
  container->_previous = NULL; container->_saved_top = NULL;
  container->_space_id = (char)(exact_log2((intptr_t) _type->value()));
#ifdef ASSERT
  container->_scanned_flag = -1;
#endif

  // Here, the container pointer is installed on the RegionData object that manages
  // the address range this container spans during OldGC. This is for fast access
  // during summarize and update of the containers top pointers.
  if (container != NULL) {
    if (container_type() == KlassRegionMap::region_start_ptr()) {
      _manager->mark_container(container);
    }
    PSParallelCompact::install_container_in_region(container);
    _manager->allocate_block(container->_start);
    if (BDAllocationVerboseLevel > 1) {
      print_allocation(container);
    }
  }
}

//
// ITERATION AND VERIFICATION
//
void
MutableBDASpace::CGRPSpace::object_iterate_containers(ObjectClosure * cl)
{
  if (container_count() > 0) {
    assert (_containers->peek() != NULL, "GenQueue malformed. Should not be empty");
    for (GenQueueIterator<container_t, mtGC> iterator = _containers->iterator();
         *iterator != NULL;
         ++iterator) {
      container_t c = *iterator;
      HeapWord * p = c->_start;
      HeapWord * t = c->_top;
      while (p < t) {
        cl->do_object(oop(p));
        p += oop(p)->size();
      }
    }
  }
}

void
MutableBDASpace::CGRPSpace::oop_iterate_containers(ExtendedOopClosure * cl)
{
  if (container_count() > 0) {
    assert (_containers->peek() != NULL, "GenQueue malformed. Should not be empty");
    for (GenQueueIterator<container_t, mtGC> iterator = _containers->iterator();
         *iterator != NULL;
         ++iterator) {
      container_t c = *iterator;
      HeapWord * p = c->_start;
      HeapWord * t = c->_top;
      while (p < t) {
        p += oop(p)->oop_iterate(cl);
      }
    }
  }
}

void
MutableBDASpace::CGRPSpace::oop_iterate_no_header_containers(OopClosure * cl)
{
  if (container_count() > 0) {
    assert (_containers->peek() != NULL, "GenQueue malformed. Should not be empty");
    for (GenQueueIterator<container_t, mtGC> iterator = _containers->iterator();
         *iterator != NULL;
         ++iterator) {
      container_t c = *iterator;
      HeapWord * p = c->_start;
      HeapWord * t = c->_top;
      while (p < t) {
        p += oop(p)->oop_iterate_no_header(cl);
      }
    }
  }
}

void
MutableBDASpace::CGRPSpace::verify()
{
  if (container_type() == KlassRegionMap::region_start_ptr()) {
    this->space()->verify();
  } else if (space()->not_empty()) {
    for(GenQueueIterator<container_t, mtGC> iterator = _containers->iterator();
        *iterator != NULL;
        ++iterator) {
      container_t c = *iterator;
      HeapWord * p = c->_start;
      HeapWord * t = c->_top;
      while(p < t) {
        oop(p)->verify();
        p += oop(p)->size();
      }
      guarantee( p == c->_top, "end of last object must match end of allocated space");
    }
  }
}

#ifdef BDA_PARANOID
void
MutableBDASpace::CGRPSpace::verify_segments_othergen() const
{
  for (GenQueueIterator<container_t, mtGC> it = _containers->iterator();
       *it != NULL; ++it ) {
    container_t const c = *it;
    if (c->_next_segment != NULL && c->_next_segment->_space_id == 0) {
      gclog_or_tty->print_cr("(PARANOID) Segment " INTPTR_FORMAT " in space " INT32_FORMAT
                             " still contains a reference to " INTPTR_FORMAT
                             " in space 0",
                             (intptr_t)c, (int)c->_space_id, (intptr_t)c->_next_segment);
    }
  }
}
#endif

void
MutableBDASpace::CGRPSpace::print_container_fragmentation_stats() const
{
  double avg = 0.0;
  double var = 0.0;
  double dev = 0.0;
  int    i   = 1;
  for (GenQueueIterator<container_t, mtGC> it = _containers->iterator();
       *it != NULL;
       ++it )
  {
    container_t const c = *it;
    HeapWord *  const bot = c->_start;
    HeapWord *  const top = c->_top;
    HeapWord *  const end = c->_end;

    // Compute fragmentation for this segment
    double frag = 1 - (pointer_delta (top, bot) / (double)pointer_delta (end, bot));
    
    // Compute the average segment fragmentation and variance.
    // Both the average and the variance are calculated using the
    // incremental method in a similar fashion to what Donald Knuth presents of
    // B. P. Welford's method.

    double const old_avg = avg;

    avg += (frag - old_avg) / i;

    // Don't compute variance if i < 2
    if ( i > 1 ) {
      var += (frag - old_avg) * (frag - avg);
    }

    i++;
  }

  var /= i - 1;
  dev = sqrt(var);
  
  // Print the statistical information
  gclog_or_tty->print_cr("Space " INT32_FORMAT, exact_log2((intptr_t)container_type()->value()));
  gclog_or_tty->print_cr("  Average fragmentation = %f%%", avg * (float)100);
  gclog_or_tty->print_cr("  Variance in fragmentation = %f", var);
  gclog_or_tty->print_cr("  Standard Deviation in fragmentation = %f", dev);
  gclog_or_tty->print_cr("  Number of segments in space = " INT32_FORMAT, container_count());
}

void
MutableBDASpace::CGRPSpace::print_allocation(container_t c, bool large) const
{
  if (large)
    gclog_or_tty->print(" %-25s", "LARGE Container Alloc");
  else
    gclog_or_tty->print(" %-25s", "Container Alloc");
  
  gclog_or_tty->print(" Size " SIZE_FORMAT "K", pointer_delta(c->_end, c->_start)/K);
  gclog_or_tty->print_cr(" [ " INTPTR_FORMAT ", " INTPTR_FORMAT ")",
                         c->_start, c->_end);
}

void
MutableBDASpace::CGRPSpace::print_allocation_stats(outputStream * st) const
{  
  st->print_cr(" %-25s Space ID = " INT32_FORMAT " allocated " INT32_FORMAT " segments during GC]",
               "--[BDA Alloc Stats ::", this->container_type()->value(), _segments_since_last_gc);
}

void
MutableBDASpace::CGRPSpace::print_used(outputStream * st) const
{
  int n = 1;
  
  if (container_count() == 0)
    return;

  st->print   ("\n");
  st->print_cr(" --[%-20s Space ID = " INT32_FORMAT " containers :]",
               "BDA Containers on", container_type()->value());
  for (GenQueueIterator<container_t, mtGC> it = _containers->iterator();
       *it != NULL;
       ++it )
  {
    container_t c = *it;

    // Don't print segment extensions of the parent alone, they'll be printed in the loop
    if (c->_prev_segment != NULL) {
      continue;
    }

    st->print_cr("  Container (" INT32_FORMAT ")", n++);
    int k = 1;
    do {
      const size_t used = pointer_delta (c->_top, c->_start);
      st->print_cr ("    Segment (" INT32_FORMAT "): Used " SIZE_FORMAT
                    "K [ " INTPTR_FORMAT ", " INTPTR_FORMAT ", " INTPTR_FORMAT ")",
                    k++, used / K, c->_start, c->_top, c->_end);
    } while ((c = c->_next_segment) != NULL);
  }
}

void
MutableBDASpace::CGRPSpace::print_container_list(bool verbose) const
{
  if ( container_count() == 0 ) return;

  gclog_or_tty->print_cr ("[container list] remove_end = " INTPTR_FORMAT
                          " ; insert_end = " INTPTR_FORMAT,
                          first_container(),
                          last_container());
  if (verbose) {
    for (GenQueueIterator<container_t, mtGC> it = _containers->iterator();
         *it != NULL;
         ++it) {
      gclog_or_tty->print(INTPTR_FORMAT " -> ", (intptr_t)*it);
    }
    gclog_or_tty->print_cr(" ");
  }
  

}

void
MutableBDASpace::CGRPSpace::reset_stats()
{
  _segments_since_last_gc = 0;
}

#ifdef ASSERT
void
MutableBDASpace::CGRPSpace::print_container_contents(outputStream * st) const
{
  assert ( container_count() > 0, "Shouldn't be possible" );
  st->print_cr ("%-30s " INT32_FORMAT, "Containers Segments for Space",
                exact_log2((intptr_t)container_type()->value()));
  {
    ResourceMark rm;
    
    for (GenQueueIterator<container_t, mtGC> it = _containers->iterator();
         *it != NULL;
         ++it )
    {
      container_t c = *it;
      if (c->_prev_segment == NULL) {
        int segment_n = 0;
        st->print_cr ("  %s (" PTR_FORMAT ")", "Container", c);
        while (c != NULL) {
          st->print_cr ("   " INT32_FORMAT " %s [" PTR_FORMAT ", " PTR_FORMAT ", " PTR_FORMAT ")",
                        segment_n, "segment", c->_start, c->_top, c->_end);
          HeapWord *  const t = c->_top;
          HeapWord *        p = c->_start;
          while (p < t) {
            oop obj = (oop)p;
            st->print_cr("    %-35s size: " INT32_FORMAT " words (" INT32_FORMAT " bytes)",
                         obj->klass()->external_name(),
                         obj->size(), obj->size() << LogHeapWordSize);
            p += obj->size();
          }
          c = c->_next_segment;
          segment_n++;
        }
      }
    }
  }
}

void
MutableBDASpace::CGRPSpace::verbose_print_all_containers (outputStream * st) const
{
  assert ( container_count() > 0, "Should not be possible." );
  st->print_cr ("%-30s " INT32_FORMAT, "Segment chain for Space",
                exact_log2((intptr_t)container_type()->value()));

  for (GenQueueIterator<container_t, mtGC> it = _containers->iterator();
       *it != NULL;
       ++it) {
    container_t c = *it;
    if (c->_prev_segment == NULL) {
      st->print("C ");
    }
    st->print ("(" PTR_FORMAT ")", c);

    // Now iterate the following segments
    container_t next_s = c;
    while ((next_s = next_s->_next_segment) != NULL) {
      st->print (" -> (" PTR_FORMAT ")", next_s);
    }
    
    st->print_cr (" ::");
    st->print_cr ("%-8s", "\\|/");
  }
}
#endif // ASSERT


////////////////// /////////////// //////////////////
////////////////// MutableBDASpace //////////////////
////////////////// /////////////// //////////////////
MutableBDASpace::MutableBDASpace(size_t alignment, ObjectStartArray * start_array)
  : MutableSpace(alignment) {
  int n_regions = KlassRegionMap::number_bdaregions() + 1;
  _spaces = new (ResourceObj::C_HEAP, mtGC) GrowableArray<CGRPSpace*>(n_regions, true);
  _page_size = os::vm_page_size();
  _start_array = start_array;

  // Initialize these to the values on the launch args
  CGRPSpace::dnf = BDAElementNumberFields;
  CGRPSpace::default_collection_size = BDACollectionSize;
  CGRPSpace::delegation_level = BDADelegationLevel;
  CGRPSpace::node_fields = ContainerNodeFields;
  // and set the regular segment size
  CGRPSpace::segment_sz = CGRPSpace::calculate_reserved_sz();
    
  // Initializing the array of collection types.
  // It must be done in the constructor due to the resize calls.
  // The number of regions must always include at least one region (the general one).
  // It is implied that the KlassRegionMap must be initialized before since it parses
  // the BDAKlasses string.
  BDARegion* region = KlassRegionMap::region_start_ptr();
  spaces()->append(new CGRPSpace(alignment, region, this)); ++region;
  for(int i = 1; i < n_regions; i++)  {
    spaces()->append(new CGRPSpace(alignment, region, this));
    region += 2;
  }
}

MutableBDASpace::~MutableBDASpace() {
  for (int i = 0; i < spaces()->length(); i++) {
    delete spaces()->at(i);
  }
  delete spaces();
}

void
MutableBDASpace::initialize(MemRegion mr, bool clear_space,
                            bool mangle_space, bool setup_pages) {

  HeapWord* bottom = mr.start();
  HeapWord* end = mr.end();

  // This means that allocations have already taken place
  // and that this is a resize
  if(!clear_space) {
    update_layout(mr);
    return;
  }

  // Set the whole space limits
  set_bottom(bottom);
  set_end(end);

  size_t space_size = pointer_delta(end, bottom);
  initialize_regions(space_size, bottom, end);
  // always clear space for new allocations
  clear(mangle_space);
}

bool
MutableBDASpace::post_initialize()
{
  assert (Universe::heap()->kind() == CollectedHeap::ParallelScavengeHeap, "Sanity");
 
  ParallelScavengeHeap * heap = ParallelScavengeHeap::heap();
  MemRegion reserved = heap->reserved_region();
  
  if (!_segment_bitmap.initialize(reserved)) {
    vm_shutdown_during_initialization(
      err_msg("Unable to allocate " SIZE_FORMAT "KB bitmaps for segment processing in "
              "garbage collection for the requested " SIZE_FORMAT "KB heap.",
              _segment_bitmap.reserved_byte_size()/K, reserved.byte_size()/K));
    return false;
  }

  // Update the filler_header_size
  _filler_header_size = align_object_size(typeArrayOopDesc::header_size(T_INT));
  
  return true;
}

container_t
MutableBDASpace::container_for_addr(HeapWord * addr)
{
  CGRPSpace * grp = _spaces->at(grp_index_contains_obj(addr));
  return grp->get_container_with_addr(addr);
}

void
MutableBDASpace::add_to_pool(container_t c, uint id)
{
  CGRPSpace * grp = _spaces->at(id);
  grp->add_to_pool(c);
}

bool
MutableBDASpace::adjust_layout(bool force)
{
  // We must force the layout to change and give space to the maximum occupying
  // region
  if(force) {
    const float multiplier = 10.5;
    const float diff_threshold = 0.7;
    // Scan the regions to find the needy one
    int i = -1;
    double max = 0.0;
    for(int j = 0; j < spaces()->length(); ++j) {
      MutableSpace* spc = spaces()->at(j)->space();
      double occupancy_ratio =
        (double)spc->used_in_words() / spc->capacity_in_words();
      double free_ratio = 1 - occupancy_ratio;
      if(occupancy_ratio - free_ratio > max) {
        i = j;
        max = occupancy_ratio - free_ratio;
      }
    }
    // Do nothing if all were max occupied
    if(i == -1)
      return false;

    MutableSpace* expand_spc = spaces()->at(i)->space();
    double occ_ratio0 =
      (double)expand_spc->used_in_words() / expand_spc->capacity_in_words();
    // check if it's worth expanding or we just let a collection do the work
    if (occ_ratio0 < 0.8)
      return true;

    double free_ratio0 = 1 - occ_ratio0;
    const size_t free_space0_sz = expand_spc->free_in_words();
    size_t expand_size = size_t(occ_ratio0 * multiplier * MinRegionSize);
    // Scan the regions to find the first one that has enough space for the
    // needy one.
    int k = -1;
    double free_ratio1 = 0.0;
    size_t free_space1_sz = 0;
    for(int j = i + 1; j < spaces()->length(); ++j) {
      MutableSpace* spc = spaces()->at(j)->space();
      free_ratio1 =
        (double)spc->free_in_words() / spc->capacity_in_words();
      free_space1_sz = spc->free_in_words();
      if(free_ratio1 - free_ratio0 > diff_threshold) {
        if((free_space1_sz / 2 > expand_size) &&
           (free_space1_sz / 2 > MinRegionSize)) {
          expand_size = free_space1_sz / 2;
          k = j;
          break;
        }
      } else {
        if((free_space1_sz > expand_size) &&
           (pointer_delta(spc->end(), spc->top() + expand_size) >= (j - i) * MinRegionSize)) {
          k = j;
          break;
        }
      }
    }
    // Do nothing if the neighbouring regions have no space to share
    if(k == -1)
      return false;

    MutableSpace* to_spc = spaces()->at(k)->space();
    HeapWord* new_end =
      (HeapWord*)round_to((intptr_t)(to_spc->top() + expand_size),
                          MinRegionSizeBytes);
    HeapWord* new_top = to_spc->top() == to_spc->bottom() ?
      expand_spc->top() :
      to_spc->top();

    if(new_top > expand_spc->top()) {
      // CollectedHeap::fill_with_object(expand_spc->top(),
      //                                 pointer_delta(new_top, expand_spc->top()),
      //                                 false);
    }

    increase_space_set_top(expand_spc, pointer_delta(new_end, expand_spc->end()),
                           new_top);

    size_t available_space_size = pointer_delta(to_spc->end(), expand_spc->end());
    initialize_regions_evenly(i + 1, k, expand_spc->end(), to_spc->end(),
                              available_space_size);

    return true;
  } else {
    // Not yet implemented
    return false;
  }
}

size_t
MutableBDASpace::compute_avg_freespace() {
  // Scan the regions to find the needy one
  int i = -1;
  double max = 0.0;
  for(int j = 0; j < spaces()->length(); ++j) {
    MutableSpace* spc = spaces()->at(j)->space();
    double occupancy_ratio =
      (double)spc->used_in_words() / spc->capacity_in_words();
    double free_ratio = 1 - occupancy_ratio;
    if(occupancy_ratio - free_ratio > max) {
      i = j;
      max = occupancy_ratio - free_ratio;
    }
  }
  // If all were occupied then return the same free size
  if ( i == -1 )
    return MutableSpace::free_in_bytes();

  size_t free_sz = MutableSpace::free_in_bytes() - free_in_bytes(i);
  return MutableSpace::free_in_bytes() + (free_sz / (spaces()->length() - 1));
}

// TODO: FIXME: THIS NEEDS TO BE FIXED
void
MutableBDASpace::update_layout(MemRegion new_mr) {
  // This is an expand
  if(new_mr.end() > end()) {
    size_t expand_size = pointer_delta(new_mr.end(), end());
    // First we expand the last region (the other space), only then we update the layout
    // This allow the following algorithm to check the borders without
    // repeating operations.
    MutableSpace* last_space = spaces()->at(0)->space();
    last_space->initialize(MemRegion(last_space->bottom(), new_mr.end()),
                           SpaceDecorator::DontClear,
                           SpaceDecorator::DontMangle);
  }
  // this is a shrink
  else {
    // size_t shrink_size = pointer_delta(end(), new_mr.end());
    // // Naive implementation...
    // int last = spaces()->length() - 1;
    // if(spaces()->at(last)->space()->free_in_bytes() > shrink_size) {
    //   shrink_space_end_noclear(spaces()->at(last)->space(), shrink_size);
    // } else {
    //   // raise the assertion fault below
    // }
  }

  // Assert before leaving and set the whole space pointers
  // TODO: Fix the order of the regions
  // int j = 0;
  // for(; j < spaces()->length(); ++j) {
  //   assert(spaces()->at(j)->space()->capacity_in_words() >= MinRegionSize,
  //          "segment is too short");
  // }
  // assert(spaces()->at(1)->space()->bottom() == new_mr.start() &&
  //        spaces()->at(j - 1)->space()->end() == new_mr.end(), "just checking");

  set_bottom(new_mr.start());
  set_end(new_mr.end());
}

void
MutableBDASpace::increase_space_noclear(MutableSpace* spc, size_t sz)
{
  spc->initialize(MemRegion(spc->bottom(), spc->end() + sz),
                  SpaceDecorator::DontClear,
                  SpaceDecorator::DontMangle);
}

void
MutableBDASpace::increase_space_set_top(MutableSpace* spc,
                                        size_t sz,
                                        HeapWord* new_top)
{
  spc->initialize(MemRegion(spc->bottom(),
                            spc->end() + sz),
                  SpaceDecorator::DontClear,
                  SpaceDecorator::DontMangle);
  spc->set_top(new_top);
}

void
MutableBDASpace::shrink_space_clear(MutableSpace* spc,
                                    size_t new_size) {
  spc->initialize(MemRegion(spc->bottom() + new_size, spc->end()),
                  SpaceDecorator::Clear,
                  SpaceDecorator::Mangle);
}

void
MutableBDASpace::shrink_space_noclear(MutableSpace* spc,
                                      size_t new_size)
{
  spc->initialize(MemRegion(spc->bottom() + new_size, spc->end()),
                  SpaceDecorator::DontClear,
                  SpaceDecorator::DontMangle);
}

void
MutableBDASpace::shrink_space_end_noclear(MutableSpace* spc,
                                          size_t shrink_size)
{
  spc->initialize(MemRegion(spc->bottom(), spc->end() - shrink_size),
                  SpaceDecorator::DontClear,
                  SpaceDecorator::DontMangle);
}


// void
// MutableBDASpace::shrink_region_set_top(int n, size_t sz, HeapWord* new_top)
// {
//   MutableSpace* space = spaces()->at(n)->space();
//   space->initialize(MemRegion(space->bottom() + sz, space->end()),
//                     SpaceDecorator::DontClear,
//                     SpaceDecorator::DontMangle);
//   space->set_top(new_top);
// }

void
MutableBDASpace::move_space_resize(MutableSpace* spc,
                                   HeapWord* to_ptr,
                                   size_t new_size)
{
  assert((intptr_t)(to_ptr + new_size) % page_size() == 0, "bottom not page aligned");
  spc->initialize(MemRegion(to_ptr, new_size),
                  SpaceDecorator::DontClear,
                  SpaceDecorator::DontMangle);
}

// deprecated
void
MutableBDASpace::initialize_regions_evenly(int from_id, int to_id,
                                           HeapWord* start_limit,
                                           HeapWord* end_limit,
                                           size_t space_size)
{
  assert(space_size % MinRegionSize == 0, "space_size not region aligned");

  const int len = (to_id - from_id) + 1;
  const int blocks_n = space_size / MinRegionSize;
  const int blocks_per_spc = (int)ceil((float)blocks_n / len);
  size_t chunk = blocks_per_spc * MinRegionSize;
  //size_t chunk = align_size_down((intptr_t)space_size / len, MinRegionSizeBytes);

  int blocks_left = blocks_n;
  HeapWord *start = start_limit, *tail = start_limit + chunk;
  for(int i = from_id; i <= to_id; ++i) {
    blocks_left -= blocks_per_spc;
    if(blocks_left < 0) {
      tail -= -blocks_left * MinRegionSize;
    }

    assert(pointer_delta(tail, start) % page_size() == 0, "chunk size not page aligned");
    spaces()->at(i)->space()->initialize(MemRegion(start, tail),
                                         SpaceDecorator::Clear,
                                         SpaceDecorator::Mangle);
    start = tail;
    tail = start + chunk;
  }
  assert(blocks_left <= 0, "some blocks are left for use");
}

void
MutableBDASpace::initialize_regions(size_t space_size,
                                    HeapWord* start,
                                    HeapWord* end)
{
  assert(space_size % MinRegionSize == 0, "space_size not region aligned");

  const int    bda_nregions = spaces()->length() - 1;
  const size_t alignment    = MutableBDASpace::CGRPSpace::segment_sz;

  // Careful with the abusers
  if (BDARatio < 1) {
    vm_shutdown_during_initialization(err_msg("The BDARatio cannot go below 1!"));
  }
  
  size_t bda_space    = (size_t)align_size_down((intptr_t)(space_size / BDARatio),
                                                (intptr_t)alignment);
  size_t bda_region_sz = bda_nregions > 0 ?
    (size_t)align_size_down((intptr_t)(bda_space / bda_nregions),
                            (intptr_t)alignment) : 0;

  // just in case some one abuses of the ratio
  int k = 1;
  while(bda_region_sz > 0 && bda_region_sz < alignment) {
    bda_space = (size_t)align_size_down((intptr_t)(space_size / BDARatio - k),
                                        (intptr_t)alignment);
    bda_region_sz = (size_t)align_size_down((intptr_t)(bda_space / bda_nregions),
                                            (intptr_t)alignment);
    k++;
  }

  HeapWord *aux_end, *aux_start = start;
  int sp_len = spaces()->length();
  for(int reg = sp_len - 1; reg > 0; reg--) {
    aux_end = aux_start + bda_region_sz;
    assert(pointer_delta(aux_end, aux_start) % page_size() == 0, "region size not page aligned");
    spaces()->at(sp_len - reg)->space()->initialize(MemRegion(aux_start, aux_end),
                                                    SpaceDecorator::Clear,
                                                    SpaceDecorator::Mangle);
    aux_start = aux_end;
  }

  // The "other" (0) space is located in the end of the old_space
  spaces()->at(0)->space()->initialize(MemRegion(aux_start, end),
                                       SpaceDecorator::Clear,
                                       SpaceDecorator::Mangle);
  
}

void
MutableBDASpace::expand_region_to_neighbour(int i, size_t expand_size)
{
  if(i == spaces()->length() - 1) {
    // it was already pushed
    return;
  }

  // This is the space that must grow
  MutableSpace* space = spaces()->at(i)->space();

  // Sometimes this is called for no strong reason
  // and we can avoid expanding to trigger an OldGC soon
  if(pointer_delta(space->end(), space->top()) >= expand_size)
    return;

  // This is to fill any leftovers of space. Don't zap!!!
  size_t remainder = pointer_delta(space->end(), space->top());
  if(remainder > CollectedHeap::min_fill_size()) {
    CollectedHeap::fill_with_object(space->top(), remainder, false);
  }

  // We grow into the neighbouring space, if possible...
  MutableSpace* neighbour_space = spaces()->at(i+1)->space();
  HeapWord* neighbour_top = neighbour_space->top();
  HeapWord* neighbour_end = neighbour_space->end();
  HeapWord* neighbour_bottom = neighbour_space->bottom();

  // ... always leaving a MinRegionSize to cope with the compactor later on
  // see PSParallelCompact::summarize_spaces_quick()
  if(pointer_delta(neighbour_end, neighbour_top) >= expand_size + MinRegionSize) {
    grow_through_neighbour(space, neighbour_space, expand_size);
    return;
  } else if(pointer_delta(neighbour_end, neighbour_top) >= expand_size) {
    // neighbour has enough space, but it should not be compacted for less than
    // then MinRegionSize value between its bottom and end values.
    // Therefore, we push expanding region end forward and then fit the
    // neighbours. But first we try to fit the neighbour in its neighbour,
    // before increasing the space or there'd be problems in reversing the op
    // in case the function return false.
    if(try_fitting_on_neighbour(i + 1)) {
      increase_space_set_top(space, pointer_delta(neighbour_end, neighbour_bottom),
                             neighbour_top);
    } else {
      // Nothing to do then trigger a collection
      return;
    }
  } else {
    // Neighbour does not have enough space, thus we grow into its neighbours.
    // The neighbour chosen must have enough space for MinRegionSize on each
    // region in between and the expand_size.
    // This j value is sure to be in bounds since the above is always true
    // for the last-1 region
    int j = i + 2;
    bool found = false;
    for(; j < spaces()->length(); ++j) {
      if(spaces()->at(j)->space()->free_in_words() >= expand_size + (j - i) * MinRegionSize)
        found = true;
      break;
    }

    // There's no space so trigger a collection
    if(!found)
      return;

    // There is space, so expand the space and shrink all neighbours evenly
    MutableSpace* to_space = spaces()->at(j)->space();
    HeapWord* new_end = (HeapWord*)round_to((intptr_t)(to_space->top() + expand_size),
                                            MinRegionSizeBytes);
    increase_space_set_top(space, pointer_delta(new_end, space->end()), to_space->top());


    size_t available_space_size = pointer_delta(to_space->end(), space->end());
    initialize_regions_evenly(j, i + 1, space->end(), to_space->end(),
                              available_space_size);
  }
}

bool
MutableBDASpace::try_fitting_on_neighbour(int moved_id)
{
  // First check the regions that may contain enough space for both
  int to_id = -1;
  for(int j = moved_id + 1; j < spaces()->length(); ++j)  {
    if(spaces()->at(j)->space()->capacity_in_words() >= 2 * MinRegionSize) {
      to_id = j;
      break;
    }
  }
  if(to_id == -1)
    return false;

  // Now we can try fit the moved space on the to-space, taking into account
  // the old occupations that they had.
  MutableSpace* moved_space = spaces()->at(moved_id)->space();
  MutableSpace* to_space = spaces()->at(to_id)->space();

  const double moved_occupancy_ratio =
    (double)moved_space->used_in_words() / moved_space->capacity_in_words();
  // TODO: Do we account to the expasion done earlier, if to_space is the last???
  const double to_occupancy_ratio =
    (double)to_space->used_in_words() / to_space->capacity_in_words();
  const double moved_free_ratio = 1 - moved_occupancy_ratio;
  const double to_free_ratio = 1 - to_occupancy_ratio;

  const size_t to_space_capacity = to_space->capacity_in_words();

  // the spaces that each region will end up occupying
  size_t new_moved_space_sz = 0;
  size_t new_to_space_sz = 0;

  // This means that at least it fits on the neighbour with its old occupancy
  // and that it can use a portion of the neighbour's space.
  if (moved_occupancy_ratio + to_occupancy_ratio <= 1) {
    if (moved_occupancy_ratio >= to_occupancy_ratio) {
      new_moved_space_sz = MAX2((size_t)(moved_occupancy_ratio * to_space_capacity),
                                MinRegionSize);
    } else {
      new_moved_space_sz = MAX2((size_t)(to_free_ratio * to_space_capacity),
                                MinRegionSize);

    }
  } else {
    //const double diff = moved_free_ratio - to_free_ration;
    // means that one of the regions is at least > 0.5 occupied

    // ..and that is the to-space
    if(moved_free_ratio >= to_free_ratio) {
      new_moved_space_sz = MAX2((size_t)(to_free_ratio * to_space_capacity),
                                MinRegionSize);
    } else {
      new_moved_space_sz = MAX2((size_t)(moved_occupancy_ratio * to_space_capacity),
                                MinRegionSize);
    }
  }

  HeapWord* aligned_limit =
    (HeapWord*)round_to((intptr_t)(to_space->bottom() + new_moved_space_sz),
                        MinRegionSizeBytes);
  new_to_space_sz = pointer_delta(to_space->end(), aligned_limit);
  if(new_to_space_sz < MinRegionSize)
    return false;

  // just checking
  assert(new_moved_space_sz >= MinRegionSize &&
         new_to_space_sz >= MinRegionSize, "size too short");

  // This is already aligned
  move_space_resize(moved_space, to_space->bottom(),
                    pointer_delta(aligned_limit, to_space->bottom()));
  if (moved_space->contains(to_space->top())) {
    moved_space->set_top(to_space->top());
    shrink_space_clear(to_space, moved_space->capacity_in_words());
  } else {
    moved_space->set_top(moved_space->end());
    shrink_space_noclear(to_space, moved_space->capacity_in_words());
  }

  assert(to_space->capacity_in_words() == new_to_space_sz, "checking sizing");
  return true;
}

void
MutableBDASpace::grow_through_neighbour(MutableSpace* growee,
                                        MutableSpace* eaten,
                                        size_t expand_size)
{
  growee->initialize(MemRegion(growee->bottom(),
                               (HeapWord*)round_to((intptr_t)(eaten->top() + expand_size),
                                                   MinRegionSizeBytes)),
                     SpaceDecorator::DontClear,
                     SpaceDecorator::DontMangle);
  growee->set_top(eaten->top());

  // Mangle to prevent following possible objects and to raise errors
  // fast in case something is wrong.
  eaten->initialize(MemRegion(growee->end(),
                              eaten->end()),
                    SpaceDecorator::Clear,
                    SpaceDecorator::Mangle);
}

void
MutableBDASpace::merge_regions(int growee, int eaten) {
  MutableSpace* growee_space = spaces()->at(growee)->space();
  MutableSpace* eaten_space = spaces()->at(eaten)->space();

  growee_space->initialize(MemRegion(growee_space->bottom(), eaten_space->end()),
                           SpaceDecorator::DontClear,
                           SpaceDecorator::DontMangle);
  growee_space->set_top(eaten_space->top());
}

void
MutableBDASpace::shrink_and_adapt(int grp) {
  MutableSpace* grp_space = spaces()->at(grp)->space();
  grp_space->initialize(MemRegion(grp_space->end(), (size_t)0),
                        SpaceDecorator::Clear,
                        SpaceDecorator::DontMangle);
  // int max_grp_id = 0;
  // size_t max = 0;
  // for(int i = 0; i < spaces()->length(); ++i) {
  //   if(MAX2(free_in_words(i), max) != max)
  //     max_grp_id = i;
  // }

  // MutableSpace* max_grp_space = spaces()->at(max_grp_id)->space();
  // MutableSpace* grp_space = spaces()->at(grp)->space();

  // size_t sz_to_set = 0;
  // if((sz_to_set = max_grp_space->free_in_words() / 2) > OldPLABSize) {
  //   HeapWord* aligned_top = (HeapWord*)round_to((intptr_t)max_grp_space->top() + sz_to_set, page_size());
  //   // should already be aligned
  //   HeapWord* aligned_end = max_grp_space->end();
  //   max_grp_space->initialize(MemRegion(max_grp_space->bottom(), aligned_top),
  //                             SpaceDecorator::DontClear,
  //                             SpaceDecorator::DontMangle);
  //   grp_space->initialize(MemRegion(aligned_top, aligned_end),
  //                         SpaceDecorator::Clear,
  //                         SpaceDecorator::DontMangle);
  // } else {
  //   grp_space->initialize(MemRegion(max_grp_space->end(), (size_t)0),
  //                         SpaceDecorator::Clear,
  //                         SpaceDecorator::Mangle);
  // }
}

/* --------------------------------------------- */
/* Commented for later use, in case it is needed */
//
// void
// MutableBDASpace::expand_region_to_neighbour(int i, size_t expand_size) {
//   if(i == spaces()->length() - 1) {
//     // it was already pushed
//     return;
//   }

//   // This is the space that must grow
//   CGRPSpace* region = spaces()->at(i);
//   MutableSpace* space = region->space();
//   HeapWord* end = space->end();

//   // This is to fill any leftovers of space
//   size_t remainder = pointer_delta(space->end(), space->top());
//   if(remainder > CollectedHeap::min_fill_size()) {
//     CollectedHeap::fill_with_object(space->top(), remainder);
//   }

//   // We grow into the neighbouring space, if possible
//   MutableSpace* neighbour_space = spaces()->at(i+1)->space();
//   HeapWord* neighbour_top = neighbour_space->top();
//   HeapWord* neighbour_end = neighbour_space->end();
//   HeapWord* neighbour_bottom = neighbour_space->bottom();

//   HeapWord* next_end = neighbour_top + expand_size;
//   if(next_end <= neighbour_end) {
//     // Expand exhausted region...
//     increase_region_noclear(i, pointer_delta(next_end - end));

//     // ... and shrink the neighbour, clearing it in the process
//     shrink_region_clear(i + 1, pointer_delta(next_end - neighbour_bottom));

//   } else {
//     if(i < spaces()->length() - 2) {
//       expand_overflown_neighbour(i + 1, expand_size);
//     }
//     // if(pointer_delta(neighbour_end, neighbour_top) >= OldPLABSize) {
//     //   next_end = neighbour_top + pointer_delta(neighbour_end, neighbour_top);
//     //   space->initialize(MemRegion(bottom, next_end),
//     //                     SpaceDecorator::DontClear,
//     //                     SpaceDecorator::DontMangle);

//     //   neighbour_space->initialize(MemRegion(next_end, (size_t)0),
//     //                               SpaceDecorator::Clear,
//     //                               SpaceDecorator::DontMangle);
//     //   // expand only the last one
//     //   int last = spaces()->length() - 1;
//     //   expand_region_to_neighbour(last,
//     //                              spaces()->at(last)->space()->bottom(),
//     //                              expand_size);
//     // } else {
//     //   size_t delta = pointer_delta(neighbour_end, neighbour_top);
//     //   if(delta > CollectedHeap::min_fill_size()) {
//     //     CollectedHeap::fill_with_object(neighbour_top, delta);
//     //   }
//     //   next_end = expand_overflown_neighbour(i + 1, expand_size);
//     //   space->initialize(MemRegion(bottom, next_end),
//     //                     SpaceDecorator::DontClear,
//     //                     SpaceDecorator::DontMangle);

//     //   neighbour_space->initialize(MemRegion(next_end, (size_t)0),
//     //                               SpaceDecorator::Clear,
//     //                               SpaceDecorator::DontMangle);
//     //   // expand only the last one
//     //   int last = spaces()->length() - 1;
//     //   expand_region_to_neighbour(last,
//     //                              spaces()->at(last)->space()->bottom(),
//     //                              expand_size);
//     }

//   }

//   set_bottom(spaces()->at(0)->space()->bottom());
//   set_end(spaces()->at(spaces()->length() - 1)->space()->end());
// }

// HeapWord*
// MutableBDASpace::expand_overflown_neighbour(int i, size_t expand_sz) {
//   HeapWord* flooded_region = spaces()->at(i + 2)->space();
//   HeapWord* flooded_top = flooded_region->top();
//   HeapWord* flooded_end = flooded_region->end();

//   HeapWord* overflown_region = spaces()->at(i + 1)->space();
//   HeapWord* overflown_top = overflown_region->top();
//   HeapWord* overflown_end = overflown_region->end();

//   if(flooded_top + expand_sz <= flooded_end) {
//     HeapWord* ret = flooded_top + expand_sz;
//     overflown_region->initialize(MemRegion(ret, (size_t)0),
//                                  SpaceDecorator::Clear,
//                                  SpaceDecorator::Mangle);
//     flooded_region->initialize(MemRegion(ret, flooded_end),
//                                SpaceDecorator::DontClear,
//                                SpaceDecorator::DontMangle);
//     return ret;
//   } else {

//   }
// }

void
MutableBDASpace::select_limits(MemRegion mr, HeapWord **start, HeapWord **tail) {
  HeapWord *old_start = mr.start();
  HeapWord *old_end = mr.end();

  *start = (HeapWord*)round_to((intptr_t)old_start, MinRegionSizeBytes);
  *tail = (HeapWord*)round_down((intptr_t)old_end, MinRegionSizeBytes);
}

size_t
MutableBDASpace::used_in_words() const {
  size_t s = 0;
  for (int i = 0; i < spaces()->length(); i++) {
    s += spaces()->at(i)->used_in_words();
  }
  return s;
}

size_t
MutableBDASpace::free_in_words() const {
  size_t s = 0;
  for (int i = 0; i < spaces()->length(); i++) {
    s += spaces()->at(i)->free_in_words();
  }
  return s;
}

size_t
MutableBDASpace::free_in_bytes() const
{
  int i = 0; size_t ret = 0;
  while (i < spaces()->length()) {
    ret += spaces()->at(i)->fast_free_in_bytes();
    i++;
  }
  return ret;
}

size_t
MutableBDASpace::free_in_words(int grp) const {
  assert(grp < spaces()->length(), "Sanity");
  return spaces()->at(grp)->free_in_words();
}

size_t
MutableBDASpace::free_in_bytes(int grp) const {
  assert(grp < spaces()->length(), "Sanity");
  return spaces()->at(grp)->free_in_bytes();
}

size_t
MutableBDASpace::capacity_in_words(Thread *thr) const {
  guarantee(thr != NULL, "No thread");
  BDARegion* ctype = thr->alloc_region();
  int i = spaces()->find(&ctype, CGRPSpace::equals);
  if( i == -1 )
    i = 0;

  return spaces()->at(i)->space()->capacity_in_words();
}

size_t
MutableBDASpace::tlab_capacity(Thread *thr) const {
  guarantee(thr != NULL, "No thread");
  BDARegion* ctype = thr->alloc_region();
  int i = spaces()->find(&ctype, CGRPSpace::equals);
  if( i == -1 )
    i = 0;

  return spaces()->at(i)->space()->capacity_in_bytes();
}

size_t
MutableBDASpace::tlab_used(Thread *thr) const {
  guarantee(thr != NULL, "No thread");
  BDARegion* ctype = thr->alloc_region();
  int i = spaces()->find(&ctype, CGRPSpace::equals);
  if( i == -1 )
    i = 0;

  return spaces()->at(i)->space()->used_in_bytes();
}

size_t
MutableBDASpace::unsafe_max_tlab_alloc(Thread *thr) const {
  guarantee(thr != NULL, "No thread");
  BDARegion* ctype = thr->alloc_region();
  int i = spaces()->find(&ctype, CGRPSpace::equals);
  if( i == -1 )
    i = 0;

  return spaces()->at(i)->space()->free_in_bytes();
}

// // // // // // // // // //
//   ALLOCATION FUNCTIONS  //
// // // // // // // // // //
HeapWord* MutableBDASpace::allocate(size_t size) {
  HeapWord *obj = cas_allocate(size);
  return obj;
}

HeapWord* MutableBDASpace::cas_allocate(size_t size) {

  // Default to 0 always. The others only allocate through the
  // allocate_container and allocate_new_segment calls
  CGRPSpace* cs = spaces()->at(0);
  MutableSpace* ms = cs->space();
  HeapWord* obj = ms->cas_allocate(size);

  if(obj != NULL) {
    HeapWord* region_top, *cur_top;
    while((cur_top = top()) < (region_top = ms->top())) {
      if( Atomic::cmpxchg_ptr(region_top, top_addr(), cur_top) == cur_top ) {
        break;
      }
    }
  } else {
    return NULL;
  }

  assert(obj <= ms->top() && obj + size <= top(), "Incorrect push of the space's top");

  return obj;
}

container_t
MutableBDASpace::allocate_container(size_t size, BDARegion* r)
{
  int i = spaces()->find(r, CGRPSpace::equals);
  assert(i > 0, "Containers can only be allocated in bda spaces already initialized");
  CGRPSpace * cs = spaces()->at(i);
  container_t new_ctr = cs->push_container(size);

  // If it failed to allocate a container in the specified space
  // then allocate a container in the "other" space.
  if (new_ctr == NULL) {
    new_ctr = spaces()->at(0)->push_container(size);
  }

  return new_ctr;
}

//
// 
HeapWord*
MutableBDASpace::allocate_element(size_t size, container_t& container)
{
  HeapWord * old_top;
  container_t segment = container;
  
  // Jump to the last segment first and update the arg in the meanwhile.
  do {
    // and try to allocate the element
    while ((old_top = segment->_top) + size < segment->_end) {
      HeapWord * new_top = old_top + size;
      if ((HeapWord*)Atomic::cmpxchg_ptr(new_top, &(segment->_top), old_top) == old_top) {
        allocate_block (old_top);
        return old_top;
      }
    }
  } while ((segment = segment->_next_segment) != NULL && (container = segment));
  
  // Which space was this container allocated?
  CGRPSpace * grp = spaces()->at((int)container->_space_id);
  assert (grp != NULL, "container must have been allocated in one of the groups");
  assert (container != NULL, "container cannot be null during allocation");
  old_top = grp->allocate_new_segment(size, container); // reuse the variable

  // Force allocate in the general object space if it wasn't possible on the bda-space
  if (old_top == NULL) {
    old_top = spaces()->at(0)->allocate_new_segment(size, container);
  }
  
  return old_top;
}

HeapWord *
MutableBDASpace::allocate_plab (container_t& container)
{
  HeapWord *  old_top;
  container_t segment = container;
  // Jump to the last segment first and update the arg in the meanwhile.
  do {
    // and try to allocate the plab
    while ((old_top = segment->_top) + BDAOldPLABSize < segment->_end) {
      HeapWord * new_top = old_top + BDAOldPLABSize;
      if ((HeapWord*)Atomic::cmpxchg_ptr(new_top, &(segment->_top), old_top) == old_top) {
        allocate_block (old_top);
        return old_top;
      }
    }
  } while ((segment = segment->_next_segment) != NULL && (container = segment));

  // Which space was this container allocated?
  CGRPSpace * grp = spaces()->at((int)container->_space_id);
  assert (grp != NULL, "The container must have been allocated in one of the groups");
  old_top = grp->allocate_new_segment(BDAOldPLABSize, container); // reuse the variable

  // Force allocate in the general object space if it wasn't possible on the bda-space
  if (old_top == NULL) {
    old_top = spaces()->at(0)->allocate_new_segment(BDAOldPLABSize, container);
  }

  // The container now belongs to this thread only (the one executing this code).
  // Therefore, it needs no further CAS pushing a LAB reserved space.
  
  return old_top;
}
//////////////// END OF ALLOCATION FUNCTIONS ////////////////

void
MutableBDASpace::clear_delete_containers_in_space(uint space_id)
{
  assert (space_id == 0, "should not be called to clear the segments of bda spaces.");
  assert (SafepointSynchronize::is_at_safepoint(), "must be at the safepoint.");
  assert (Thread::current() == (Thread*)VMThread::vm_thread(), "must be called by the VM thread");
  assert (UseBDA, "must be using the bda extensions");
  
  // Free all container segments allocated
  CGRPSpace * grp = spaces()->at(space_id);
  grp->clear_delete_containers();

  // Unset all bits in the segment_bitmap
  HeapWord * const bottom = grp->space()->bottom();
  HeapWord * const top    = grp->space()->top();
  _segment_bitmap.clear_range(_segment_bitmap.addr_to_bit(bottom),
                              _segment_bitmap.addr_to_bit(top));
}

#ifdef ASSERT
void
MutableBDASpace::reset_grp_stats()
{
  for (int i = 0; i < spaces()->length(); ++i) {
    spaces()->at(i)->reset_stats();
  }
}
#endif

void MutableBDASpace::clear(bool mangle_space)
{
  MutableSpace::clear(mangle_space);
  for(int i = 0; i < spaces()->length(); ++i) {
    spaces()->at(i)->space()->clear(mangle_space);
  }
}

void
MutableBDASpace::set_top(HeapWord* value) {
  for (int i = 0; i < spaces()->length(); ++i) {
    MutableSpace *ms = spaces()->at(i)->space();

    if(ms->contains(value)) {
      // The pushing of the top from the summarize phase can create
      // a hole of invalid heap area. See ParallelCompactData::summarize
      // if(ms->top() < value) {
      //   const size_t delta = pointer_delta(value, ms->top());
      //   if(delta > CollectedHeap::min_fill_size()) {
      //     CollectedHeap::fill_with_object(ms->top(), delta, false);
      //   }
      // }
      ms->set_top(value);
    }
  }
  MutableSpace::set_top(value);
}

void
MutableBDASpace::set_top_for_allocations() {
  MutableSpace::set_top_for_allocations();
}

void
MutableBDASpace::set_top_for_allocations(HeapWord *p) {
  MutableSpace::set_top_for_allocations(p);
}

void
MutableBDASpace::print_on(outputStream* st) const {
  st->print(" %s", "space for BDAGen [");
  MutableSpace::print_on(st); st->print_cr("]");
  for (int i = 0; i < spaces()->length(); ++i) {
    st->print ("   %s " INT32_FORMAT " [", "Space ID =", i);
    spaces()->at(i)->space()->print_on(st);
  }
#ifdef ASSERT
  if (BDAPrintAllContainers && Verbose) {
    verbose_print_all_containers();
  }
#endif // ASSERT
      
      

}

void
MutableBDASpace::print_short_on(outputStream* st) const {
  st->print_cr(" %-15s ", "BDA OldGen");
  MutableSpace::print_short_on(st);
  for(int i = 0; i < spaces()->length(); ++i) {
    spaces()->at(i)->space()->print_short_on(st);
  }
}

/// ITERATION
void
MutableBDASpace::oop_iterate(ExtendedOopClosure* cl)
{
  // First iterate the containers of all spaces,
  // then iterate the objects in the other-space that
  // do not belong to containers.
  for(int i = 0; i < spaces()->length(); ++i) {
    CGRPSpace * grp = spaces()->at(i);
    if (grp->container_count() > 0) {
      grp->oop_iterate_containers(cl);
    }
  }

  MutableSpace * const other_space = spaces()->at(0)->space();
  HeapWord * const top = other_space->top();
  HeapWord * bottom    = other_space->bottom();

  while (bottom < top) {
    HeapWord * const tmp_top = this->get_next_beg_seg(bottom, top);
    HeapWord * p = bottom;
    while (p < tmp_top) {
      p += oop(p)->oop_iterate(cl);
    }
    HeapWord * const new_bottom = this->get_next_end_seg(tmp_top + 1, top) + 1;
    bottom = new_bottom;
  }
}

void
MutableBDASpace::oop_iterate_no_header(OopClosure* cl)
{
  // First iterate the containers of all spaces,
  // then iterate the objects in the other-space that
  // do not belong to containers
  for(int i = 0; i < spaces()->length(); ++i) {
    CGRPSpace * grp = spaces()->at(i);
    if (grp->container_count() > 0) {
      grp->oop_iterate_no_header_containers(cl);
    }
  }

  MutableSpace * const other_space = spaces()->at(0)->space();
  HeapWord * const top = other_space->top();
  HeapWord * bottom    = other_space->bottom();

  while (bottom < top) {
    HeapWord * const tmp_top = this->get_next_beg_seg(bottom, top);
    HeapWord * p = bottom;
    while (p < tmp_top) {
      p += oop(p)->oop_iterate_no_header(cl);
    }
    HeapWord * const new_bottom = this->get_next_end_seg(tmp_top + 1, top) + 1;
    bottom = new_bottom;
  }
}

void
MutableBDASpace::object_iterate(ObjectClosure* cl)
{
  // First iterate the containers of all spaces,
  // then iterate the objects in the other-space that
  // do not belong to containers
  for(int i = 0; i < spaces()->length(); ++i) {
    CGRPSpace * grp = spaces()->at(i);
    if (grp->container_count() > 0) {
      grp->object_iterate_containers(cl);
    }
  }

  MutableSpace * const other_space = spaces()->at(0)->space();
  HeapWord * const top = other_space->top();
  HeapWord * bottom    = other_space->bottom();

  while (bottom < top) {
    HeapWord * const tmp_top = this->get_next_beg_seg(bottom, top);
    HeapWord * p = bottom;
    while (p < tmp_top) {
      cl->do_object(oop(p));
      p += oop(p)->size();
    }
    HeapWord * const new_bottom = this->get_next_end_seg(tmp_top + 1, top) + 1;
    bottom = new_bottom;
  }
}
/////////////

void
MutableBDASpace::print_object_space() const
{
  ResourceMark rm(Thread::current());
  // TODO: FIXME: Implement a way to group objects of the same class and print a summarization
  // of the whole space, objects included.
  // if(descriptive) {
  //   int j = only_collections ? 1 : 0;
  //   for(; j < spaces()->length(); ++j) {
  //     CGRPSpace* grp = spaces()->at(j);
  //     MutableSpace* spc = grp->space();
  //     BDARegion* region = grp->container_type();
  //     gclog_or_tty->print_cr("\nRegion for objects %s :: From 0x%x to 0x%x top 0x%x",
  //                            region->toString(),
  //                            spc->bottom(),
  //                            spc->end(),
  //                            spc->top());
  //     gclog_or_tty->print_cr("\t Fillings (words): Capacity %d :: Used space %d :: Free space %d",
  //                   spc->capacity_in_words(),
  //                   spc->used_in_words(),
  //                   spc->free_in_words());
  //     gclog_or_tty->print_cr("\t Space layout:");
  //     oop next_obj = (oop)spc->bottom();
  //     while((HeapWord*)next_obj < spc->top()) {
  //       Klass* klassPtr = next_obj->klass();
  //       gclog_or_tty->print_cr("address: %x , klass: %s",
  //                     next_obj,
  //                     klassPtr->external_name());
  //       next_obj = (oop)((HeapWord*)next_obj + next_obj->size());
  //     }
  //   }
  // } 
  // else {
  // for(int j = 0; j < spaces()->length(); ++j) {
  //     CGRPSpace* grp = spaces()->at(j);
  //     MutableSpace* spc = grp->space();
  //     BDARegion* region = grp->container_type();
  //     gclog_or_tty->print("\nRegion for objects %x", region->value());
  //     gclog_or_tty->print_cr(":: From ["
  //                            INTPTR_FORMAT ") to ["
  //                            INTPTR_FORMAT ")\t top = "
  //                            INTPTR_FORMAT,
  //                            spc->bottom(),
  //                            spc->end(),
  //                            spc->top());
  //     gclog_or_tty->print_cr("\t :: Fillings (words): Capacity " SIZE_FORMAT "K"
  //                            "   :: Used space " SIZE_FORMAT "K"
  //                            "   :: Free space " SIZE_FORMAT "K",
  //                            spc->capacity_in_words() / K,
  //                            spc->used_in_words() / K,
  //                            spc->free_in_words() / K);
  //   }
  // }
  for (int i = 0; i < spaces()->length(); ++i) {
    spaces()->at(i)->print_used(gclog_or_tty);
  }
}

void
MutableBDASpace::print_spaces_fragmentation_stats() const
{
  gclog_or_tty->print_cr("--[BDA Spaces fragmentation stats:]");
  
  for (int i = 0; i < spaces()->length(); ++i ) {

    CGRPSpace * grp = spaces()->at(i);

    // Test if it's worth continuing and warn for containers in space 0
    if ( i == 0 && grp->container_count() > 0 ) {
      gclog_or_tty->print_cr("  (warning: Containers in Space 0  --"
                             "  Segment Count = " INT32_FORMAT ")",
                             grp->container_count());
    } else if ( grp->container_count() == 0 ) continue;

    // asserts
    assert ( grp->container_count() > 0, "Shouldn't reach here without containers" );

    // Compute the average segment fragmentation and variance.
    // Both the average and the variance are calculated using the
    // incremental method in a similar fashion to what Donald Knuth presents of
    // B. P. Welford's method.
    grp->print_container_fragmentation_stats();
  }
}


void
MutableBDASpace::print_allocation_stats() const
{
  assert (BDAllocationVerboseLevel > 0, "Shouldn't reach here with 0 verbosity level");

  // if (BDAllocationVerboseLevel > 1) {
  
  // } else
  if (BDAllocationVerboseLevel > 0) {
    for (int i = 0; i < spaces()->length(); ++i) {
      CGRPSpace * grp = spaces()->at(i);
      grp->print_allocation_stats(gclog_or_tty);
    }
  }
}

#ifdef ASSERT
void
MutableBDASpace::print_spaces_contents() const
{
  gclog_or_tty->print_cr("--[BDA Spaces Contents (WARNING: Very verbose):]");

  for (int i = 0; i < spaces()->length(); ++i) {

    CGRPSpace * grp = spaces()->at(i);

    if ( grp->container_count() == 0 ) continue;

    // asserts
    assert ( grp->container_count() > 0, "Shouldn't reach here without containers" );

    grp->print_container_contents (gclog_or_tty);
  }
}

void
MutableBDASpace::verbose_print_all_containers() const
{
  // Verbose flag must be turned on.
  assert (Verbose, "Verbose flag should be turned on");

  for (int i = 0; i < spaces()->length(); ++i) {

    CGRPSpace * grp = spaces()->at(i);
    
    if ( grp->container_count() == 0 ) continue;

    assert ( grp->container_count() > 0, "Shouldn't reach here without containers");

    grp->verbose_print_all_containers (gclog_or_tty);
  }
}
#endif // ASSERT

void
MutableBDASpace::verify() {
  for(int i = 0; i < spaces()->length(); ++i) {
    CGRPSpace* grp = spaces()->at(i);
    grp->verify();
  }
}

#ifdef BDA_PARANOID
void
MutableBDASpace::verify_segments_in_othergen() const
{
  for (int i = 1; i < spaces()->length(); i++) {
    spaces()->at(i)->verify_segments_othergen();
  }
}
#endif
/**
 * STATISTICS FUNCTIONS
 */

// avg_nsegments_in_bda() reports the average number of container segments
// in the bda-spaces, i.e., all except the space id=0 (the other object space).
float
MutableBDASpace::avg_nsegments_in_bda()
{
  uint acc = 0;
  for(int i = 1; i < spaces()->length(); ++i) {
    acc += spaces()->at(i)->container_count();
  }
  return acc / (float)(spaces()->length() - 1);
}

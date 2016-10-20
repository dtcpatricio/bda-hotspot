# include "bda/mutableBDASpace.inline.hpp"
# include "gc_implementation/shared/spaceDecorator.hpp"
# include "gc_implementation/parallelScavenge/parallelScavengeHeap.hpp"
# include "gc_implementation/parallelScavenge/psParallelCompact.hpp"
# include "memory/resourceArea.hpp"
# include "runtime/thread.hpp"
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
size_t MutableBDASpace::CGRPSpace::segment_sz = 0;

/////////////// ////////////////// ///////////////
/////////////// BDACardTableHelper ////////////////
/////////////// ////////////////// /////////////////
BDACardTableHelper::BDACardTableHelper(MutableBDASpace* sp) {
  _length = sp->container_count();
  _containers = NEW_RESOURCE_ARRAY(container_helper_t, _length);

  // Fill in the array. It is filled by each CGRPSpace, i.e., the manager of each bda space.
  int j = 0; int i = 0;
  while (j++ < sp->spaces()->length() - 1) {
    MutableBDASpace::CGRPSpace * grp = sp->spaces()->at(j);
    if (grp->space()->not_empty())
      grp->save_top_ptrs(_containers, &i);
  }
}

BDACardTableHelper::~BDACardTableHelper()
{
  FREE_RESOURCE_ARRAY(container_helper_t, _containers, _length);  
}

HeapWord *
BDACardTableHelper::top(container_t * c)
{
  for (int i = 0; i < _length; ++i) {
    if (_containers[i]._container == c) {
      return _containers[i]._top;
    }
  }
  return NULL;
}

//////////// ////////////////////////// //////////
//////////// MutableBDASpace::CGRPSpace //////////
//////////// ////////////////////////// //////////
container_t *
MutableBDASpace::CGRPSpace::allocate_container(size_t size)
{  
  container_t * container = install_container_in_space(CGRPSpace::segment_sz, size);
  // Here, the container pointer is installed on the RegionData object that manages
  // the address range this container spans during OldGC. This is for fast access
  // during summarize and update of the containers top pointers.
  if (container != NULL)
    PSParallelCompact::install_container_in_region(container);
  
  return container;
}

container_t *
MutableBDASpace::CGRPSpace::allocate_large_container(size_t size)
{
  size_t reserved_sz = calculate_large_reserved_sz(size);
  container_t * container = install_container_in_space(reserved_sz, size);
  // Here, the container pointer is installed on the RegionData object that manages
  // the address range this container spans during OldGC. This is for fast access
  // during summarize and update of the containers top pointers.
  if (container != NULL)
    PSParallelCompact::install_container_in_region(container);
  
  return container;
}

//
// ITERATION AND VERIFICATION
//
void
MutableBDASpace::CGRPSpace::object_iterate_containers(ObjectClosure * cl)
{
  if (container_count() > 0) {
    assert (_containers->peek() != NULL, "GenQueue malformed. Should not be empty");
    for (GenQueueIterator<container_t*, mtGC> iterator = _containers->iterator();
         *iterator != NULL;
         ++iterator) {
      container_t * c = *iterator;
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
    for (GenQueueIterator<container_t*, mtGC> iterator = _containers->iterator();
         *iterator != NULL;
         ++iterator) {
      container_t * c = *iterator;
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
    for (GenQueueIterator<container_t*, mtGC> iterator = _containers->iterator();
         *iterator != NULL;
         ++iterator) {
      container_t * c = *iterator;
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
    for(GenQueueIterator<container_t*, mtGC> iterator = _containers->iterator();
        *iterator != NULL;
        ++iterator) {
      container_t * c = *iterator;
      HeapWord * p = c->_start;
      HeapWord * t = c->_top;
      while(p < t) {
        oop(p)->verify();
        p += oop(p)->size();
      }
      guarantee( p == c->_top, "end of last object must match end of space");
    }
  }
}

////////////////// /////////////// //////////////////
////////////////// MutableBDASpace //////////////////
////////////////// /////////////// //////////////////
MutableBDASpace::MutableBDASpace(size_t alignment) : MutableSpace(alignment) {
  int n_regions = KlassRegionMap::number_bdaregions();
  _spaces = new (ResourceObj::C_HEAP, mtGC) GrowableArray<CGRPSpace*>(n_regions, true);
  _page_size = os::vm_page_size();

  // Initialize these to the values on the launch args
  CGRPSpace::dnf = BDAElementNumberFields;
  CGRPSpace::default_collection_size = BDACollectionSize;
  CGRPSpace::delegation_level = BDADelegationLevel;
  // and set the regular segment size
  CGRPSpace::segment_sz = CGRPSpace::calculate_reserved_sz();
    
  // Initializing the array of collection types.
  // It must be done in the constructor due to the resize calls.
  // The number of regions must always include at least one region (the general one).
  // It is implied that the KlassRegionMap must be initialized before since it parses
  // the BDAKlasses string.
  BDARegion* region = KlassRegionMap::region_start_ptr();
  spaces()->append(new CGRPSpace(alignment, region)); ++region;
  for(int i = 0; i < n_regions; i++)  {
    spaces()->append(new CGRPSpace(alignment, region));
    region += 2;
  }
}

MutableBDASpace::~MutableBDASpace() {
  for (int i = 0; i < spaces()->length(); i++) {
    delete spaces()->at(i);
  }
  delete spaces();
}

void MutableBDASpace::initialize(MemRegion mr, bool clear_space, bool mangle_space, bool setup_pages) {
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

  return true;
}

container_t *
MutableBDASpace::container_for_addr(HeapWord * addr)
{
  CGRPSpace * grp = _spaces->at(grp_index_contains_obj(addr));
  return grp->get_container_with_addr(addr);
}

void
MutableBDASpace::add_to_pool(container_t * c, uint id)
{
  CGRPSpace * grp = _spaces->at(id);
  grp->add_to_pool(c);
}

void
MutableBDASpace::set_shared_gc_pointers()
{
  for (int i = 0; i < spaces()->length(); ++i) {
    spaces()->at(i)->set_shared_gc_pointer();
  }
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

void
MutableBDASpace::update_layout(MemRegion new_mr) {
  Thread* thr = Thread::current();
  BDARegion* last_region = thr->alloc_region();
  int i = spaces()->find(&last_region, CGRPSpace::equals);

  // This is an expand
  if(new_mr.end() > end()) {
    size_t expand_size = pointer_delta(new_mr.end(), end());
    // First we expand the last region, only then we update the layout
    // This allow the following algorithm to check the borders without
    // repeating operations.
    MutableSpace* last_space = spaces()->at(spaces()->length() - 1)->space();
    last_space->initialize(MemRegion(last_space->bottom(), new_mr.end()),
                           SpaceDecorator::DontClear,
                           SpaceDecorator::DontMangle);
    //increase_space_noclear(last_space, expand_size);
    // Now we expand the region that exhausted its space to the neighbours
    //expand_region_to_neighbour(i, expand_size);
  }
  // this is a shrink
  else {
    size_t shrink_size = pointer_delta(end(), new_mr.end());
    // Naive implementation...
    int last = spaces()->length() - 1;
    if(spaces()->at(last)->space()->free_in_bytes() > shrink_size) {
      shrink_space_end_noclear(spaces()->at(last)->space(), shrink_size);
    } else {
      // raise the assertion fault below
    }
  }

  // Assert before leaving and set the whole space pointers
  // TODO: Fix the order of the regions
  int j = 0;
  for(; j < spaces()->length(); ++j) {
    assert(spaces()->at(j)->space()->capacity_in_words() >= MinRegionSize,
           "segment is too short");
  }
  assert(spaces()->at(1)->space()->bottom() == new_mr.start() &&
         spaces()->at(j - 1)->space()->end() == new_mr.end(), "just checking");

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
  
  size_t bda_space    = (size_t)round_down(space_size / BDARatio, alignment);
  size_t bda_region_sz = (size_t)round_down(bda_space / bda_nregions, alignment);

  // just in case some one abuses of the ratio
  int k = 1;
  while(bda_region_sz < alignment) {
    bda_space = (size_t)round_down(space_size / BDARatio - k, alignment);
    bda_region_sz = (size_t)round_down(bda_space / bda_nregions, alignment);
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
    s += spaces()->at(i)->space()->used_in_words();
  }
  return s;
}

size_t
MutableBDASpace::free_in_words() const {
  size_t s = 0;
  for (int i = 0; i < spaces()->length(); i++) {
    s += spaces()->at(i)->space()->free_in_words();
  }
  return s;
}

size_t
MutableBDASpace::free_in_words(int grp) const {
  assert(grp < spaces()->length(), "Sanity");
  return spaces()->at(grp)->space()->free_in_words();
}

size_t
MutableBDASpace::free_in_bytes(int grp) const {
  assert(grp < spaces()->length(), "Sanity");
  return spaces()->at(grp)->space()->free_in_bytes();
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
  Thread* thr = Thread::current();
  BDARegion* type2aloc = thr->alloc_region();

  int i = spaces()->find(&type2aloc, CGRPSpace::equals);

  if (i == 0)
    HeapWord * dummy = 0x0;
  // default to the no-collection space
  if (i == -1)
    i = 0;

  CGRPSpace* cs = spaces()->at(i);
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

container_t *
MutableBDASpace::allocate_container(size_t size, BDARegion* r)
{
  int i = spaces()->find(r, CGRPSpace::equals);
  assert(i > 0, "Containers can only be allocated in bda spaces already initialized");
  CGRPSpace * cs = spaces()->at(i);
  container_t * new_ctr = cs->push_container(size);

  // If it failed to allocate a container in the specified space
  // then allocate a container in the "other" space.
  if (new_ctr == NULL) {
    new_ctr = spaces()->at(0)->push_container(size);
  }

  if (new_ctr != NULL)
    mark_container(new_ctr);
  return new_ctr;
}

//
// This is lock and atomic-construct free because container_t structs are handled
// by each GC thread seperately. If StealTasks are implemented and some kind of
// synchronization is required, then implement the bumping pointer with a CAS.
HeapWord*
MutableBDASpace::allocate_element(size_t size, container_t ** c)
{
  HeapWord * old_top = (*c)->_top;
  HeapWord * new_top = old_top + size;
  if (new_top < (*c)->_end) {
    (*c)->_top = new_top;
  } else {
    // If it fails to allocate in the container, i.e., it is full, then
    // allocate a new segment of the container and change the argument passed
    // accordingly.

    // Which space was this container allocated?
    CGRPSpace * grp = NULL;
    for (int i = 0; i < _spaces->length(); ++i) {
      if (_spaces->at(i)->space()->contains((*c)->_start)) {
        grp = _spaces->at(i); break;
      }
    }
    assert (grp != NULL, "The container must have been allocated in one of the groups");
    old_top = grp->allocate_new_segment(size, c); // reuse the variable
    // Force allocate in the general object space if it wasn't possible on the bda-space
    if (old_top == NULL) {
      old_top = spaces()->at(0)->allocate_new_segment(size, c);
    }

    // Now mark the container if the allocation was successful.
    if (old_top != NULL)
      mark_container(*c);
    
  }

  return old_top;
}

//////////////// END OF ALLOCATION FUNCTIONS ////////////////

void
MutableBDASpace::clear_delete_containers_in_space(uint space_id)
{
  assert (space_id == 0, "should not be called to clear the segments of bda spaces.");
  
  // Free all container segments allocated
  CGRPSpace * grp = spaces()->at(space_id);
  grp->clear_delete_containers();

  // Unset all bits in the segment_bitmap
  HeapWord * const bottom = grp->space()->bottom();
  HeapWord * const top    = grp->space()->top();
  _segment_bitmap.clear_range(_segment_bitmap.addr_to_bit(bottom),
                              _segment_bitmap.addr_to_bit(top));
}

void MutableBDASpace::clear(bool mangle_space)
{
  MutableSpace::clear(mangle_space);
  for(int i = 0; i < spaces()->length(); ++i) {
    spaces()->at(i)->space()->clear(mangle_space);
  }
}

bool MutableBDASpace::update_top() {
  // HeapWord *curr_top = top();
  bool changed = false;
  // for(int i = 0; i < spaces()->length(); ++i) {
  //   if(spaces()->at(i)->top() > curr_top) {
  //     curr_top = spaces()->at(i)->top();
  //     changed = true;
  //   }
  // }
  // MutableSpace::set_top(curr_top);
  return changed;
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
  MutableSpace::print_on(st);
  for (int i = 0; i < spaces()->length(); ++i) {
    CGRPSpace *cs = spaces()->at(i);
    st->print("\t region for type %d", cs->container_type());
    cs->space()->print_on(st);
  }
}

void
MutableBDASpace::print_short_on(outputStream* st) const {
  MutableSpace::print_short_on(st);
  st->print(" ()");
  for(int i = 0; i < spaces()->length(); ++i) {
    st->print("region %d: ", spaces()->at(i)->container_type());
    spaces()->at(i)->space()->print_short_on(st);
    if(i < spaces()->length() - 1) {
      st->print(", ");
    }
  }
  st->print(") ");
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
MutableBDASpace::print_current_space_layout(bool descriptive,
                                            bool only_collections)
{
  ResourceMark rm(Thread::current());

  if(descriptive) {
    int j = only_collections ? 1 : 0;
    for(; j < spaces()->length(); ++j) {
      CGRPSpace* grp = spaces()->at(j);
      MutableSpace* spc = grp->space();
      BDARegion* region = grp->container_type();
      gclog_or_tty->print_cr("\nRegion for objects %s :: From 0x%x to 0x%x top 0x%x",
                             region->toString(),
                             spc->bottom(),
                             spc->end(),
                             spc->top());
      gclog_or_tty->print_cr("\t Fillings (words): Capacity %d :: Used space %d :: Free space %d",
                    spc->capacity_in_words(),
                    spc->used_in_words(),
                    spc->free_in_words());
      gclog_or_tty->print_cr("\t Space layout:");
      oop next_obj = (oop)spc->bottom();
      while((HeapWord*)next_obj < spc->top()) {
        Klass* klassPtr = next_obj->klass();
        gclog_or_tty->print_cr("address: %x , klass: %s",
                      next_obj,
                      klassPtr->external_name());
        next_obj = (oop)((HeapWord*)next_obj + next_obj->size());
      }
    }
  } else {
    for(int j = 0; j < spaces()->length(); ++j) {
      CGRPSpace* grp = spaces()->at(j);
      MutableSpace* spc = grp->space();
      BDARegion* region = grp->container_type();
      gclog_or_tty->print("\nRegion for objects %x", region->value());
      gclog_or_tty->print_cr(":: From ["
                             INTPTR_FORMAT ") to ["
                             INTPTR_FORMAT ")\t top = "
                             INTPTR_FORMAT,
                             spc->bottom(),
                             spc->end(),
                             spc->top());
      gclog_or_tty->print_cr("\t :: Fillings (words): Capacity " SIZE_FORMAT "K"
                             "   :: Used space " SIZE_FORMAT "K"
                             "   :: Free space " SIZE_FORMAT "K",
                             spc->capacity_in_words() / K,
                             spc->used_in_words() / K,
                             spc->free_in_words() / K);
    }
  }
}

void
MutableBDASpace::verify() {
  for(int i = 0; i < spaces()->length(); ++i) {
    CGRPSpace* grp = spaces()->at(i);
    grp->verify();
  }
}

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

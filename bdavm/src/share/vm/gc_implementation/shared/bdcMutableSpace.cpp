#include "gc_implementation/shared/bdcMutableSpace.hpp"
#include "gc_implementation/shared/spaceDecorator.hpp"
#include "gc_implementation/parallelScavenge/parallelScavengeHeap.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/thread.hpp"
#include "oops/oop.inline.hpp"
#include "oops/klassRegionMap.hpp"

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

// Forward declarations
class ParallelScavengeHeap;

// Definition of sizes - conformant with the PSParallelCompact class
const size_t BDCMutableSpace::Log2MinRegionSize = 16; // 64K HeapWords
const size_t BDCMutableSpace::MinRegionSize = (size_t)1 << Log2MinRegionSize;
const size_t BDCMutableSpace::MinRegionSizeBytes = MinRegionSize << LogHeapWordSize;

const size_t BDCMutableSpace::MinRegionSizeOffsetMask = MinRegionSize - 1;
const size_t BDCMutableSpace::MinRegionAddrOffsetMask = MinRegionSizeBytes - 1;
const size_t BDCMutableSpace::MinRegionAddrMask       = ~MinRegionAddrOffsetMask;

BDACardTableHelper::BDACardTableHelper(BDCMutableSpace* sp) {
  _length = sp->collections()->length();
  _tops = NEW_RESOURCE_ARRAY(HeapWord*, _length);
  _spaces = NEW_RESOURCE_ARRAY(MutableSpace*, _length);
  for(int i = 0; i < _length; ++i) {
    MutableSpace* ms = sp->collections()->at(i)->space();
    _tops[i] = ms->top();
    _spaces[i] = ms;
  }
}

BDACardTableHelper::~BDACardTableHelper()
{
  FREE_RESOURCE_ARRAY(HeapWord*, _tops, _length);
  FREE_RESOURCE_ARRAY(MutableSpace*, _spaces, _length);
}

BDCMutableSpace::BDCMutableSpace(size_t alignment) : MutableSpace(alignment) {
  _collections = new (ResourceObj::C_HEAP, mtGC) GrowableArray<CGRPSpace*>(0, true);
  _page_size = os::vm_page_size();

  // Initializing the array of collection types.
  // It must be done in the constructor due to the resize calls.
  // The number of regions must always include at least one region (the general one).
  // It is implied that the KlassRegionMap must be initialized before since it parses
  // the BDAKlasses string.
  int n_regions = KlassRegionMap::number_bdaregions() + 1;
  bdareg_t region = BDARegion::region_start;
  for(int i = 0; i < n_regions; i++)  {
    collections()->append(new CGRPSpace(alignment, region));
    region <<= BDARegion::region_shift;
  }
}

BDCMutableSpace::~BDCMutableSpace() {
  for (int i = 0; i < collections()->length(); i++) {
    delete collections()->at(i);
  }
  delete collections();
}

void BDCMutableSpace::initialize(MemRegion mr, bool clear_space, bool mangle_space, bool setup_pages) {

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
BDCMutableSpace::adjust_layout(bool force)
{
  // We must force the layout to change and give space to the maximum occupying
  // region
  if(force) {
    const float multiplier = 10.5;
    const float diff_threshold = 0.7;
    // Scan the regions to find the needy one
    int i = -1;
    double max = 0.0;
    for(int j = 0; j < collections()->length(); ++j) {
      MutableSpace* spc = collections()->at(j)->space();
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

    MutableSpace* expand_spc = collections()->at(i)->space();
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
    for(int j = i + 1; j < collections()->length(); ++j) {
      MutableSpace* spc = collections()->at(j)->space();
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

    MutableSpace* to_spc = collections()->at(k)->space();
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
BDCMutableSpace::compute_avg_freespace() {
  // Scan the regions to find the needy one
  int i = -1;
  double max = 0.0;
  for(int j = 0; j < collections()->length(); ++j) {
    MutableSpace* spc = collections()->at(j)->space();
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
  return MutableSpace::free_in_bytes() + (free_sz / (collections()->length() - 1));
}

void
BDCMutableSpace::update_layout(MemRegion new_mr) {
  Thread* thr = Thread::current();
  BDARegion last_region = thr->alloc_region();
  int i = collections()->find(&last_region, CGRPSpace::equals);

  // This is an expand
  if(new_mr.end() > end()) {
    size_t expand_size = pointer_delta(new_mr.end(), end());
    // First we expand the last region, only then we update the layout
    // This allow the following algorithm to check the borders without
    // repeating operations.
    MutableSpace* last_space = collections()->at(collections()->length() - 1)->space();
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
    int last = collections()->length() - 1;
    if(collections()->at(last)->space()->free_in_bytes() > shrink_size) {
      shrink_space_end_noclear(collections()->at(last)->space(), shrink_size);
    } else {
      // raise the assertion fault below
    }
  }

  // Assert before leaving and set the whole space pointers
  // TODO: Fix the order of the regions
  int j = 0;
  for(; j < collections()->length(); ++j) {
    assert(collections()->at(j)->space()->capacity_in_words() >= MinRegionSize,
           "segment is too short");
  }
  assert(collections()->at(0)->space()->bottom() == new_mr.start() &&
         collections()->at(j - 1)->space()->end() == new_mr.end(), "just checking");

  set_bottom(new_mr.start());
  set_end(new_mr.end());
}

void
BDCMutableSpace::increase_space_noclear(MutableSpace* spc, size_t sz)
{
  spc->initialize(MemRegion(spc->bottom(), spc->end() + sz),
                    SpaceDecorator::DontClear,
                    SpaceDecorator::DontMangle);
}

void
BDCMutableSpace::increase_space_set_top(MutableSpace* spc,
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
BDCMutableSpace::shrink_space_clear(MutableSpace* spc,
                                    size_t new_size) {
  spc->initialize(MemRegion(spc->bottom() + new_size, spc->end()),
                    SpaceDecorator::Clear,
                    SpaceDecorator::Mangle);
}

void
BDCMutableSpace::shrink_space_noclear(MutableSpace* spc,
                                      size_t new_size)
{
  spc->initialize(MemRegion(spc->bottom() + new_size, spc->end()),
                  SpaceDecorator::DontClear,
                  SpaceDecorator::DontMangle);
}

void
BDCMutableSpace::shrink_space_end_noclear(MutableSpace* spc,
                                      size_t shrink_size)
{
  spc->initialize(MemRegion(spc->bottom(), spc->end() - shrink_size),
                  SpaceDecorator::DontClear,
                  SpaceDecorator::DontMangle);
}


// void
// BDCMutableSpace::shrink_region_set_top(int n, size_t sz, HeapWord* new_top)
// {
//   MutableSpace* space = collections()->at(n)->space();
//   space->initialize(MemRegion(space->bottom() + sz, space->end()),
//                     SpaceDecorator::DontClear,
//                     SpaceDecorator::DontMangle);
//   space->set_top(new_top);
// }

void
BDCMutableSpace::move_space_resize(MutableSpace* spc,
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
BDCMutableSpace::initialize_regions_evenly(int from_id, int to_id,
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
    collections()->at(i)->space()->initialize(MemRegion(start, tail),
                                              SpaceDecorator::Clear,
                                              SpaceDecorator::Mangle);
    start = tail;
    tail = start + chunk;
  }
  assert(blocks_left <= 0, "some blocks are left for use");
}

void
BDCMutableSpace::initialize_regions(size_t space_size,
                                    HeapWord* start,
                                    HeapWord* end)                                    
{
  assert(space_size % MinRegionSize == 0, "space_size not region aligned");
  
  const int bda_nregions = collections()->length() - 1;
  size_t bda_space = (size_t)round_to(space_size / BDARegionRatio, MinRegionSize);
  size_t bda_region_sz = (size_t)round_to(bda_space / bda_nregions, MinRegionSize);

  // just in case some one abuses of the ratio
  int k = 1;
  while(bda_region_sz < MinRegionSize) {
    bda_space = (size_t)round_to(space_size / BDARegionRatio - k, MinRegionSize);
    bda_region_sz = (size_t)round_to(bda_space / bda_nregions, MinRegionSize);
    k++;
  }

  HeapWord *aux_start, *aux_end = end;
  for(int reg = collections()->length() - 1; reg > 0; reg--) {
    aux_start = aux_end - bda_region_sz;
    assert(pointer_delta(aux_end, aux_start) % page_size() == 0, "region size not page aligned");
    collections()->at(reg)->space()->initialize(MemRegion(aux_start, aux_end),
                                                SpaceDecorator::Clear,
                                                SpaceDecorator::Mangle);
    aux_end = aux_start;
  }

  collections()->at(0)->space()->initialize(MemRegion(start, aux_end),
                                            SpaceDecorator::Clear,
                                            SpaceDecorator::Mangle);
}

void
BDCMutableSpace::expand_region_to_neighbour(int i, size_t expand_size)
{
  if(i == collections()->length() - 1) {
    // it was already pushed
    return;
  }

  // This is the space that must grow
  MutableSpace* space = collections()->at(i)->space();

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
  MutableSpace* neighbour_space = collections()->at(i+1)->space();
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
    for(; j < collections()->length(); ++j) {
      if(collections()->at(j)->space()->free_in_words() >= expand_size + (j - i) * MinRegionSize)
        found = true;
      break;
    }

    // There's no space so trigger a collection
    if(!found)
      return;

    // There is space, so expand the space and shrink all neighbours evenly
    MutableSpace* to_space = collections()->at(j)->space();
    HeapWord* new_end = (HeapWord*)round_to((intptr_t)(to_space->top() + expand_size),
                                            MinRegionSizeBytes);
    increase_space_set_top(space, pointer_delta(new_end, space->end()), to_space->top());


    size_t available_space_size = pointer_delta(to_space->end(), space->end());
    initialize_regions_evenly(j, i + 1, space->end(), to_space->end(),
                              available_space_size);
  }
}

bool
BDCMutableSpace::try_fitting_on_neighbour(int moved_id)
{
  // First check the regions that may contain enough space for both
  int to_id = -1;
  for(int j = moved_id + 1; j < collections()->length(); ++j)  {
    if(collections()->at(j)->space()->capacity_in_words() >= 2 * MinRegionSize) {
      to_id = j;
      break;
    }
  }
  if(to_id == -1)
    return false;

  // Now we can try fit the moved space on the to-space, taking into account
  // the old occupations that they had.
  MutableSpace* moved_space = collections()->at(moved_id)->space();
  MutableSpace* to_space = collections()->at(to_id)->space();

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
BDCMutableSpace::grow_through_neighbour(MutableSpace* growee,
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
BDCMutableSpace::merge_regions(int growee, int eaten) {
  MutableSpace* growee_space = collections()->at(growee)->space();
  MutableSpace* eaten_space = collections()->at(eaten)->space();

  growee_space->initialize(MemRegion(growee_space->bottom(), eaten_space->end()),
                           SpaceDecorator::DontClear,
                           SpaceDecorator::DontMangle);
  growee_space->set_top(eaten_space->top());
}

void
BDCMutableSpace::shrink_and_adapt(int grp) {
  MutableSpace* grp_space = collections()->at(grp)->space();
  grp_space->initialize(MemRegion(grp_space->end(), (size_t)0),
                        SpaceDecorator::Clear,
                        SpaceDecorator::DontMangle);
  // int max_grp_id = 0;
  // size_t max = 0;
  // for(int i = 0; i < collections()->length(); ++i) {
  //   if(MAX2(free_in_words(i), max) != max)
  //     max_grp_id = i;
  // }

  // MutableSpace* max_grp_space = collections()->at(max_grp_id)->space();
  // MutableSpace* grp_space = collections()->at(grp)->space();

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
// BDCMutableSpace::expand_region_to_neighbour(int i, size_t expand_size) {
//   if(i == collections()->length() - 1) {
//     // it was already pushed
//     return;
//   }

//   // This is the space that must grow
//   CGRPSpace* region = collections()->at(i);
//   MutableSpace* space = region->space();
//   HeapWord* end = space->end();

//   // This is to fill any leftovers of space
//   size_t remainder = pointer_delta(space->end(), space->top());
//   if(remainder > CollectedHeap::min_fill_size()) {
//     CollectedHeap::fill_with_object(space->top(), remainder);
//   }

//   // We grow into the neighbouring space, if possible
//   MutableSpace* neighbour_space = collections()->at(i+1)->space();
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
//     if(i < collections()->length() - 2) {
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
//     //   int last = collections()->length() - 1;
//     //   expand_region_to_neighbour(last,
//     //                              collections()->at(last)->space()->bottom(),
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
//     //   int last = collections()->length() - 1;
//     //   expand_region_to_neighbour(last,
//     //                              collections()->at(last)->space()->bottom(),
//     //                              expand_size);
//     }

//   }

//   set_bottom(collections()->at(0)->space()->bottom());
//   set_end(collections()->at(collections()->length() - 1)->space()->end());
// }

// HeapWord*
// BDCMutableSpace::expand_overflown_neighbour(int i, size_t expand_sz) {
//   HeapWord* flooded_region = collections()->at(i + 2)->space();
//   HeapWord* flooded_top = flooded_region->top();
//   HeapWord* flooded_end = flooded_region->end();

//   HeapWord* overflown_region = collections()->at(i + 1)->space();
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
BDCMutableSpace::select_limits(MemRegion mr, HeapWord **start, HeapWord **tail) {
  HeapWord *old_start = mr.start();
  HeapWord *old_end = mr.end();

  *start = (HeapWord*)round_to((intptr_t)old_start, MinRegionSizeBytes);
  *tail = (HeapWord*)round_down((intptr_t)old_end, MinRegionSizeBytes);
}

size_t
BDCMutableSpace::used_in_words() const {
  size_t s = 0;
  for (int i = 0; i < collections()->length(); i++) {
    s += collections()->at(i)->space()->used_in_words();
  }
  return s;
}

size_t
BDCMutableSpace::free_in_words() const {
  size_t s = 0;
  for (int i = 0; i < collections()->length(); i++) {
    s += collections()->at(i)->space()->free_in_words();
  }
  return s;
}

size_t
BDCMutableSpace::free_in_words(int grp) const {
  assert(grp < collections()->length(), "Sanity");
  return collections()->at(grp)->space()->free_in_words();
}

size_t
BDCMutableSpace::free_in_bytes(int grp) const {
  assert(grp < collections()->length(), "Sanity");
  return collections()->at(grp)->space()->free_in_bytes();
}

size_t
BDCMutableSpace::capacity_in_words(Thread *thr) const {
  guarantee(thr != NULL, "No thread");
  bdareg_t ctype = thr->alloc_region();
  int i = collections()->find(&ctype, CGRPSpace::equals);
  if( i == -1 )
    i = 0;

  return collections()->at(i)->space()->capacity_in_words();
}

size_t
BDCMutableSpace::tlab_capacity(Thread *thr) const {
  guarantee(thr != NULL, "No thread");
  bdareg_t ctype = thr->alloc_region();
  int i = collections()->find(&ctype, CGRPSpace::equals);
  if( i == -1 )
    i = 0;

  return collections()->at(i)->space()->capacity_in_bytes();
}

size_t
BDCMutableSpace::tlab_used(Thread *thr) const {
  guarantee(thr != NULL, "No thread");
  bdareg_t ctype = thr->alloc_region();
  int i = collections()->find(&ctype, CGRPSpace::equals);
  if( i == -1 )
    i = 0;

  return collections()->at(i)->space()->used_in_bytes();
}

size_t
BDCMutableSpace::unsafe_max_tlab_alloc(Thread *thr) const {
  guarantee(thr != NULL, "No thread");
  bdareg_t ctype = thr->alloc_region();
  int i = collections()->find(&ctype, CGRPSpace::equals);
  if( i == -1 )
    i = 0;

  return collections()->at(i)->space()->free_in_bytes();
}

HeapWord* BDCMutableSpace::allocate(size_t size) {
  HeapWord *obj = cas_allocate(size);
  return obj;
}

HeapWord* BDCMutableSpace::cas_allocate(size_t size) {
  Thread* thr = Thread::current();
  bdareg_t type2aloc = thr->alloc_region();

  int i = collections()->find(&type2aloc, CGRPSpace::equals);

  // default to the no-collection space
  if (i == -1)
    i = 0;

  CGRPSpace* cs = collections()->at(i);
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

void BDCMutableSpace::clear(bool mangle_space) {
  MutableSpace::clear(mangle_space);
  for(int i = 0; i < collections()->length(); ++i) {
    collections()->at(i)->space()->clear(mangle_space);
  }
}

bool BDCMutableSpace::update_top() {
  // HeapWord *curr_top = top();
  bool changed = false;
  // for(int i = 0; i < collections()->length(); ++i) {
  //   if(collections()->at(i)->top() > curr_top) {
  //     curr_top = collections()->at(i)->top();
  //     changed = true;
  //   }
  // }
  // MutableSpace::set_top(curr_top);
  return changed;
}

void
BDCMutableSpace::set_top(HeapWord* value) {
  for (int i = 0; i < collections()->length(); ++i) {
    MutableSpace *ms = collections()->at(i)->space();

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
BDCMutableSpace::set_top_for_allocations() {
  MutableSpace::set_top_for_allocations();
}

void
BDCMutableSpace::set_top_for_allocations(HeapWord *p) {
  MutableSpace::set_top_for_allocations(p);
}

void
BDCMutableSpace::print_on(outputStream* st) const {
  MutableSpace::print_on(st);
  for (int i = 0; i < collections()->length(); ++i) {
    CGRPSpace *cs = collections()->at(i);
    st->print("\t region for type %d", cs->coll_type());
    cs->space()->print_on(st);
  }
}

void
BDCMutableSpace::print_short_on(outputStream* st) const {
  MutableSpace::print_short_on(st);
  st->print(" ()");
  for(int i = 0; i < collections()->length(); ++i) {
    st->print("region %d: ", collections()->at(i)->coll_type());
    collections()->at(i)->space()->print_short_on(st);
    if(i < collections()->length() - 1) {
      st->print(", ");
    }
  }
  st->print(") ");
}

/// ITERATION
void
BDCMutableSpace::oop_iterate(ExtendedOopClosure* cl)
{
  for(int i = 0; i < collections()->length(); ++i) {
    collections()->at(i)->space()->oop_iterate(cl);
  }
}

void
BDCMutableSpace::oop_iterate_no_header(OopClosure* cl)
{
  for(int i = 0; i < collections()->length(); ++i) {
    collections()->at(i)->space()->oop_iterate_no_header(cl);
  }
}

void
BDCMutableSpace::object_iterate(ObjectClosure* cl)
{
  for(int i = 0; i < collections()->length(); ++i) {
    collections()->at(i)->space()->object_iterate(cl);
  }
}
/////////////

void
BDCMutableSpace::print_current_space_layout(bool descriptive,
                                            bool only_collections)
{
  ResourceMark rm(Thread::current());

  if(descriptive) {
    int j = only_collections ? 1 : 0;
    for(; j < collections()->length(); ++j) {
      CGRPSpace* grp = collections()->at(j);
      MutableSpace* spc = grp->space();
      BDARegion region = grp->coll_type();
      gclog_or_tty->print_cr("Region for objects %s :: From 0x%x to 0x%x top 0x%x",
                             region.toString(),
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
    for(int j = 0; j < collections()->length(); ++j) {
      CGRPSpace* grp = collections()->at(j);
      MutableSpace* spc = grp->space();
      BDARegion region = grp->coll_type();
      gclog_or_tty->print_cr("Region for objects %x :: From 0x%x to 0x%x top 0x%x",
                             region.value(),
                             spc->bottom(),
                             spc->end(),
                             spc->top());
      gclog_or_tty->print_cr("\t Fillings (words): Capacity %d :: Used space %d :: Free space %d",
                    spc->capacity_in_words(),
                    spc->used_in_words(),
                    spc->free_in_words());
    }
  }
}

void
BDCMutableSpace::verify() {
  for(int i = 0; i < collections()->length(); ++i) {
    MutableSpace* ms = collections()->at(i)->space();
    ms->verify();
  }
}


// FIXME: NOT IN USE NOW. IF IT HAS TO BE UNCOMMENT AND IMPLEMENT. THIS WAS
// THE ALTERNATIVE TO PUT SpaceInfo AS A POINTER TYPE AND DYNAMICALLY ALLOCATE
// IT IN THE CHEAP.
/* ---------- Support for the Parallel Compact GC -------------- */
// void
// BDCMutableSpace::set_dense_prefix(HeapWord* addr)
// {
//   for(int i = 0; i < _collections->length(); i++) {
//     CGRPSpace* grp = _collections->at(i);
//     if ( grp->space()->contains(addr) ) {
//       grp->set_dense_prefix(addr);
//     }
//   }
// }

// void
// BDCMutableSpace::set_min_dense_prefix(HeapWord* addr)
// {
//   for(int i = 0; i < _collections->length(); i++) {
//     CGRPSpace* grp = _collections->at(i);
//     if ( grp->space()->contains(addr) ) {
//       grp->set_min_dense_prefix(addr);
//     }
//   }
// }

// void
// BDCMutableSpace::set_new_top(HeapWord* addr)
// {
//   for(int i = 0; i < _collections->length(); i++) {
//     CGRPSpace* grp = _collections->at(i);
//     if ( grp->space()->contains(addr) ) {
//       grp->set_new_top(addr);
//     }
//   }
// }

// HeapWord*
// BDCMutableSpace::dense_prefix(HeapWord* addr)
// {
//   for(int i = 0; i < _collections->length(); i++) {
//     CGRPSpace* grp = _collections->at(i);
//     if ( grp->space()->contains(addr) ) {
//       grp->dense_prefix();
//     }
//   }
// }

// HeapWord*
// BDCMutableSpace::min_dense_prefix(HeapWord* addr)
// {
//   for(int i = 0; i < _collections->length(); i++) {
//     CGRPSpace* grp = _collections->at(i);
//     if ( grp->space()->contains(addr) ) {
//       grp->min_dense_prefix();
//     }
//   }
// }

// HeapWord*
// BDCMutableSpace::new_top(HeapWord* addr)
// {
//   for(int i = 0; i < _collections->length(); i++) {
//     CGRPSpace* grp = _collections->at(i);
//     if ( grp->space()->contains(addr) ) {
//       grp->new_top();
//     }
//   }
// }

// HeapWord**
// BDCMutableSpace::new_top_addr(HeapWord* addr)
// {
//   for(int i = 0; i < _collections->length(); i++) {
//     CGRPSpace* grp = _collections->at(i);
//     if ( grp->space()->contains(addr) ) {
//       grp->new_top(addr);
//     }
//   }
// }

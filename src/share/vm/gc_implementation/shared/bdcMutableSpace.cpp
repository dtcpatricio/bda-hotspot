#include "gc_implementation/shared/bdcMutableSpace.hpp"
#include "gc_implementation/shared/spaceDecorator.hpp"
#include "gc_interface/collectedHeap.hpp"
#include "runtime/thread.hpp"

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

BDACardTableHelper::BDACardTableHelper(BDCMutableSpace* sp) {
  for(int i = 0; i < sp->collections()->length(); ++i) {
    MutableSpace* ms = sp->collections()->at(i)->space();
    _tops.append(ms->top());
    _bottoms.append(ms->bottom());
  }
}

HeapWord*
BDACardTableHelper::top_region_for_slice(HeapWord* slice_start) {
  HeapWord* p = _tops.top();
  for(int i = _tops.length() - 2; i >= 0; i--) {
    // if another saved top is smaller than the previous and slice_start is smaller
    // then update the saved top
    p = _tops.at(i) < p && slice_start < _tops.at(i) ? _tops.at(i) : p;
  }
  return p;
}

BDCMutableSpace::BDCMutableSpace(size_t alignment) : MutableSpace(alignment) {
  _collections = new (ResourceObj::C_HEAP, mtGC) GrowableArray<CGRPSpace*>(0, true);
  _page_size = os::vm_page_size();

  // initializing the array of collection types
  // maybe a prettier way of doing this...
  // It must be done in the constructor due to the resize calls
  collections()->append(new CGRPSpace(alignment, region_other));
  collections()->append(new CGRPSpace(alignment, region_hashmap));
  collections()->append(new CGRPSpace(alignment, region_hashtable));
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
  if(!clear_space && top() > this->bottom()) {
    update_layout(mr);
    return;
  }

  // Set the whole space limits
  set_bottom(bottom);
  set_end(end);

  size_t space_size = pointer_delta(end, bottom);
  int len = collections()->length();
  size_t chunk = align_size_down((intptr_t)space_size / len, _page_size);

  HeapWord *start, *tail;
  for(int i = len - 1; i > 0; --i) {
    select_limits(MemRegion(end - chunk, chunk), &start, &tail);

    assert(pointer_delta(tail, start) % page_size() == 0, "Chunk size is not page aligned");
    collections()->at(i)->space()->initialize(MemRegion(start, tail),
                                              clear_space,
                                              mangle_space);
    end = start;
  }

  select_limits(MemRegion(bottom, end), &start, &tail);

  // just in case it is not properly aligned
  if(start > bottom) {
    size_t delta = pointer_delta(start, bottom);
    const size_t min_fill_size = CollectedHeap::min_fill_size();
    if(delta > min_fill_size) {
      CollectedHeap::fill_with_object(bottom, delta);
    }
  }

  assert(pointer_delta(tail, start) % page_size() == 0, "First region is not page aligned");
  collections()->at(0)->space()->initialize(MemRegion(start, tail),
                                            clear_space,
                                            mangle_space);

  // always clear space for new allocations
  clear(mangle_space);
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
    increase_region_noclear(collections()->length() - 1, expand_size);
    // Now we expand the region that got exhausted to the neighbours
    expand_region_to_neighbour(i, expand_size);
  }
  // this is a shrink
  else {
    size_t shrink_size = pointer_delta(end(), new_mr.end());
  }

  // Assert before leaving and set the whole space pointers
  // skip for now.... TODO: Fix the order of the regions
  // int last = collections()->length() - 1;
  // assert(collections()->at(0)->space()->bottom() == new_mr.start() &&
  //        collections()->at(last)->space()->end() == new_mr.end(), "just checking");

  set_bottom(new_mr.start());
  set_end(new_mr.end());
}

void
BDCMutableSpace::increase_region_noclear(int n, size_t sz) {
  MutableSpace* space = collections()->at(n)->space();
  space->initialize(MemRegion(space->bottom(), space->end() + sz),
                    SpaceDecorator::DontClear,
                    SpaceDecorator::DontMangle);
}

void BDCMutableSpace::shrink_region_clear(int n, size_t sz) {
  MutableSpace* space = collections()->at(n)->space();
  space->initialize(MemRegion(space->bottom() + sz, space->end()),
                    SpaceDecorator::Clear,
                    SpaceDecorator::Mangle);
}


void
BDCMutableSpace::expand_region_to_neighbour(int i, size_t expand_size) {
  if(i == collections()->length() - 1) {
    // it was already pushed
    return;
  }

  // This is the space that must grow
  MutableSpace* space = collections()->at(i)->space();

  // Sometimes this is called with no reason
  if(pointer_delta(space->end(), space->top()) >= expand_size)
    return;

  // This is to fill any leftovers of space
  // size_t remainder = pointer_delta(space->end(), space->top());
  // if(remainder > CollectedHeap::min_fill_size()) {
  //   CollectedHeap::fill_with_object(space->top(), remainder);
  // }

  // We grow into the neighbouring space, if possible
  MutableSpace* neighbour_space = collections()->at(i+1)->space();
  HeapWord* neighbour_top = neighbour_space->top();
  HeapWord* neighbour_end = neighbour_space->end();
  HeapWord* neighbour_bottom = neighbour_space->bottom();

  if(pointer_delta(neighbour_end, neighbour_top) >= expand_size) {
    merge_regions(i, i+1);
    shrink_and_adapt(i+1);
    return;
  } else {
    // neighbour does not have enough space, thus we grow into its neighbours
    // this k value is sure to be in bounds since the above is always true
    // for the last-1 region
    int j = i + 2;
    for(; j < collections()->length(); ++j) {
      if(collections()->at(j)->space()->free_in_words() >= expand_size)
        break;
    }
    MutableSpace* space_to_shrink = collections()->at(j)->space();
    HeapWord* new_top = space_to_shrink->top();
    HeapWord* next_end = (HeapWord*)round_to((intptr_t)new_top + expand_size, page_size());

    space->initialize(MemRegion(space->bottom(), next_end),
                      SpaceDecorator::DontClear,
                      SpaceDecorator::DontMangle);
    space->set_top(new_top);

    space_to_shrink->initialize(MemRegion(next_end, space_to_shrink->end()),
                                SpaceDecorator::Clear,
                                SpaceDecorator::Mangle);

    for(int k = j - 1; k > i; --k) {
      collections()->at(k)->space()->initialize(MemRegion(next_end, (size_t)0),
                                                SpaceDecorator::Clear,
                                                SpaceDecorator::Mangle);
    }
  }
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

  *start = (HeapWord*)round_to((intptr_t)old_start, page_size());
  *tail = (HeapWord*)round_down((intptr_t)old_end, page_size());
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
BDCMutableSpace::capacity_in_words(Thread *thr) const {
  guarantee(thr != NULL, "No thread");
  BDARegion ctype = thr->alloc_region();
  int i = collections()->find(&ctype, CGRPSpace::equals);
  if( i == -1 )
    i = 0;

  return collections()->at(i)->space()->capacity_in_words();
}

size_t
BDCMutableSpace::tlab_capacity(Thread *thr) const {
  guarantee(thr != NULL, "No thread");
  BDARegion ctype = thr->alloc_region();
  int i = collections()->find(&ctype, CGRPSpace::equals);
  if( i == -1 )
    i = 0;

  return collections()->at(i)->space()->capacity_in_bytes();
}

size_t
BDCMutableSpace::tlab_used(Thread *thr) const {
  guarantee(thr != NULL, "No thread");
  BDARegion ctype = thr->alloc_region();
  int i = collections()->find(&ctype, CGRPSpace::equals);
  if( i == -1 )
    i = 0;

  return collections()->at(i)->space()->used_in_bytes();
}

size_t
BDCMutableSpace::unsafe_max_tlab_alloc(Thread *thr) const {
  guarantee(thr != NULL, "No thread");
  BDARegion ctype = thr->alloc_region();
  int i = collections()->find(&ctype, CGRPSpace::equals);
  if( i == -1 )
    i = 0;

  return collections()->at(i)->space()->free_in_bytes();
}

HeapWord* BDCMutableSpace::allocate(size_t size) {
  // Thread* thr = Thread::current();
  // CollectionType type2aloc = thr->alloc_region();
  // // should we sanitize here the type2aloc in case it has wrong values

  // int i = 0;//= collections()->find(&type2aloc, CGRPSpace::equals);

  // // default to the no-collection space
  // if(i == -1) {
  //   i = 0;
  // }

  // CGRPSpace* cs = collections()->at(i);

  // HeapWord *cs_top = cs->top();
  // HeapWord *cs_end = cs->end();
  // HeapWord *region_top = top();
  // HeapWord *region_end = end();

  // MutableSpace::set_top(cs_top);
  // MutableSpace::set_end(cs_end);

  HeapWord *obj = cas_allocate(size);

  // MutableSpace::set_end(region_end);
  // update_top();

  return obj;
}

HeapWord* BDCMutableSpace::cas_allocate(size_t size) {
  Thread* thr = Thread::current();
  BDARegion type2aloc = thr->alloc_region();

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

   //  if( Atomic::cmpxchg_ptr(next_top, top_addr(), cur_top) == cur_top ) {

  //     obj = ms->cas_allocate(size);
  //     if(obj == NULL)
  //       return NULL;
  //     else if(obj < top() - size) {
  //       ms->set_top(top());
  //       continue;
  //     }
  //     assert(obj <= top(), "Incorrect allocation");
  //     break;
  //   }
  // } while(true);

  if(obj != NULL) {
    size_t remainder = pointer_delta(ms->end(), obj + size);
    if (remainder < CollectedHeap::min_fill_size() && remainder > 0) {
      if (ms->cas_deallocate(obj, size)) {
        // We were the last to allocate and created a fragment less than
        // a minimal object.
        return NULL;
      } else {
        guarantee(false, "Deallocation should always succeed");
      }
    }
  }

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
    HeapWord *top = MAX2((HeapWord*)round_down((intptr_t)ms->top(), page_size()),
                         ms->bottom());

    if(ms->contains(value)) {
      // The pushing of the top from the summarize phase can create
      // a hole of invalid heap area. See ParallelCompactData::summarize
      // if(ms->top() < value) {
      //   const size_t delta = pointer_delta(value, ms->top());
      //   if(delta > CollectedHeap::min_fill_size()) {
      //     CollectedHeap::fill_with_object(ms->top(), delta);
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

void BDCMutableSpace::print_short_on(outputStream* st) const {
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

void
BDCMutableSpace::verify() {
  for(int i = 0; i < collections()->length(); ++i) {
    MutableSpace* ms = collections()->at(i)->space();
    ms->verify();
  }
}

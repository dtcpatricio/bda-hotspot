#include "gc_implementation/shared/bdcMutableSpace.hpp"
#include "gc_implementation/shared/spaceDecorator.hpp"
#include "gc_interface/collectedHeap.hpp"
#include "runtime/thread.hpp"

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

BDCMutableSpace::BDCMutableSpace(size_t alignment) : MutableSpace(alignment) {
  _collections = new (ResourceObj::C_HEAP, mtGC) GrowableArray<CGRPSpace*>(0, true);
  _page_size = os::vm_page_size();
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

  // initializing the array of collection types
  // maybe a prettier way of doing this...
  collections()->append(new CGRPSpace(alignment(), coll_type_none));
  collections()->append(new CGRPSpace(alignment(), coll_type_hashmap));

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
                                              SpaceDecorator::Clear,
                                              ZapUnusedHeapArea);
    end = start;
  }

  select_limits(MemRegion(bottom, end), &start, &tail);

  // just in case it is not properly aligned
  if(start > bottom) {
    size_t delta = pointer_delta(start, bottom);
    const size_t min_fill_size = CollectedHeap::min_fill_size();
    if(delta > min_fill_size && delta > 0) {
      CollectedHeap::fill_with_object(bottom, delta);
    }
  }

  assert(pointer_delta(tail, start) % page_size() == 0, "First region is not page aligned");
  collections()->at(0)->space()->initialize(MemRegion(start, tail),
                                            SpaceDecorator::Clear,
                                            ZapUnusedHeapArea);

  // always clear space for new allocations
  clear(mangle_space);
}


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
BDCMutableSpace::capacity_in_words(Thread *thr) const {
  guarantee(thr != NULL, "No thread");
  BDACollectionType ctype = thr->curr_alloc_coll_type();
  int i = collections()->find(&ctype, CGRPSpace::equals);
  if( i == -1 )
    i = 0;

  return collections()->at(i)->space()->capacity_in_words();
}

size_t
BDCMutableSpace::tlab_capacity(Thread *thr) const {
  guarantee(thr != NULL, "No thread");
  BDACollectionType ctype = thr->curr_alloc_coll_type();
  int i = collections()->find(&ctype, CGRPSpace::equals);
  if( i == -1 )
    i = 0;

  return collections()->at(i)->space()->capacity_in_bytes();
}

size_t
BDCMutableSpace::tlab_used(Thread *thr) const {
  guarantee(thr != NULL, "No thread");
  BDACollectionType ctype = thr->curr_alloc_coll_type();
  int i = collections()->find(&ctype, CGRPSpace::equals);
  if( i == -1 )
    i = 0;

  return collections()->at(i)->space()->used_in_bytes();
}

size_t
BDCMutableSpace::unsafe_max_tlab_alloc(Thread *thr) const {
  guarantee(thr != NULL, "No thread");
  BDACollectionType ctype = thr->curr_alloc_coll_type();
  int i = collections()->find(&ctype, CGRPSpace::equals);
  if( i == -1 )
    i = 0;

  return collections()->at(i)->space()->free_in_bytes();
}

HeapWord* BDCMutableSpace::allocate(size_t size) {
  // Thread* thr = Thread::current();
  // CollectionType type2aloc = thr->curr_alloc_coll_type();
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
  BDACollectionType type2aloc = thr->curr_alloc_coll_type();

  int i = collections()->find(&type2aloc, CGRPSpace::equals);

  // default to the no-collection space
  if (i == -1)
    i = 0;

  CGRPSpace *cs = collections()->at(i);
  MutableSpace *ms = cs->space();
  HeapWord *obj;

  do {
    HeapWord *cur_top = top(), *next_top = top() + size;
    if((next_top == cur_top + size) && Atomic::cmpxchg_ptr(next_top, top_addr(), cur_top) == cur_top) {
      obj = ms->cas_allocate(size);
      if(obj == NULL)
        return NULL;
      else if(obj < top() - size) {
        ms->set_top(top());
        continue;
      }
      assert(obj <= top(), "Incorrect allocation");
      break;
    }
  } while(true);

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
  // bool found_top = false;
  // for (int i = 0; i < collections()->length(); ++i) {
  //   CGRPSpace *cs = collections()->at(i);
  //   HeapWord *top = MAX2((HeapWord*)round_down((intptr_t)cs->top(), page_size()),
  //                        cs->bottom());

  //   if(cs->contains(value)) {
  //     // We try to push the value to the next region, in case the space
  //     // left is smaller than min_fill_size. A similar interpretation is
  //     // made on mutableNUMASpace.cpp

  //     if(i < collections()->length() - 1) {
  //       size_t remainder = pointer_delta(cs->end(), value);
  //       const size_t min_fill_size = CollectedHeap::min_fill_size();
  //       if(remainder < min_fill_size && remainder > 0) {
  //         CollectedHeap::fill_with_object(value, min_fill_size);
  //         value += min_fill_size;
  //         assert(!cs->contains(value), "Value should be in the next region");
  //         continue;
  //       }
  //     }
  //     cs->set_top(value);
  //     found_top = true;
  //   } else {
  //     if(found_top) {
  //       cs->set_top(cs->bottom());
  //     } else {
  //       cs->set_top(cs->end());
  //     }
  //   }
  // }
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

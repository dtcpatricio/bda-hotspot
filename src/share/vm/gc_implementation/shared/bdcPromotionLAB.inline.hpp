#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_BDCPROMOTIONLAB_INLINE_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_BDCPROMOTIONLAB_INLINE_HPP

#include "gc_implementation/shared/bdcPromotionLAB.hpp"
#include "gc_interface/collectedHeap.inline.hpp"

HeapWord*
BDCYoungPromotionLAB::allocate(size_t size) {
  Thread *thr = Thread::current();
  CollectionType ctype = thr->curr_alloc_coll_type();

  int i = collections()->find(&ctype, PLABRegion::equals);
  assert(i != -1, "There must be at least one collection type");

  PLABRegion *r = collections()->at(i);
  HeapWord *top = r->top();
  HeapWord *end = r->end();

  HeapWord *obj = CollectedHeap::align_allocation_or_fail(top, end,
                                                          SurvivorAlignmentInBytes);

  if (obj == NULL) {
    return NULL;
  }

  HeapWord *new_top = obj + size;
  if(new_top > obj && new_top <= end) {
    r->set_top(new_top);
    assert(is_ptr_aligned(obj, SurvivorAlignmentInBytes) && is_object_aligned((intptr_t)new_top),
           "Invalid alignment");
    return obj;
  } else {
    // now it is already aligned for future allocation
    cs->set_top(obj);
    return NULL;
  }
}

#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_BDCPROMOTIONLAB_INLINE_HPP

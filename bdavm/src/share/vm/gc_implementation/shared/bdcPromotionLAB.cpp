#include "precompiled.hpp"
#include "gc_implementation/shared/bdcPromotionLAB.hpp"
#include "gc_implementation/shared/spaceDecorator.hpp"

size_t BDCPromotionLAB::filler_header_size;

// Constructor and destructor. We must make sure that _collections is never
// NULL.
BDCPromotionLAB::BDCPromotionLAB() : _top(NULL), _bottom(NULL), _end(NULL) {
  _collections = new (ResourceObj::C_HEAP, mtGC) GrowableArray<PLABRegion*>(0, true);
  _page_size = os::vm_page_size();

  // _collections->append(new PLABRegion(region_other));
  // _collections->append(new PLABRegion(region_hashmap));
}

void BDCPromotionLAB::initialize(MemRegion lab) {

  HeapWord* bottom = lab.start();
  HeapWord* end = lab.end();

  set_bottom(bottom);
  set_top(bottom);
  set_end(end);

  filler_header_size = align_object_size(typeArrayOopDesc::header_size(T_INT));

  // make sure that we cannot create regions if lab is too small
  if(pointer_delta(bottom, end) < BDAMinOldPLABSize) {
    _state = zero_size;
    return;
  }

  if(free() > 0) {
    assert(lab.word_size() % page_size() == 0, "The size of the MemRegion is not page aligned");
    size_t lab_size = pointer_delta(end, bottom);
    uint len = collections()->length();
    size_t chunk = align_size_down((intptr_t)(lab_size / len), (page_size() / sizeof(HeapWord)));

    // We start from the last regions to save more space for the first
    for(int i = len - 1; i > 0; --i) {
      MemRegion mr(end - chunk, chunk);
      assert(mr.word_size() >= filler_header_size, "Region size is too small");
      // give space for a filler object in the end of the region
      mr.set_end(mr.end() - filler_header_size);

      collections()->at(i)->initialize(mr);
      end -= chunk;
    }

    // code for the last region
    MemRegion lr(bottom, end);
    assert(lr.word_size() >= filler_header_size, "Last region size is too small");

    lr.set_end(lr.end() - filler_header_size);

    collections()->at(0)->initialize(lr);
    set_end(_end - filler_header_size);
    _state = needs_flush;
  } else {
    _state = zero_size;
  }

  assert(this->top() <= this->end(), "Top and End pointers are out of order");
}

void
BDCPromotionLAB::select_limits(MemRegion mr, HeapWord **start, HeapWord **tail) {
  HeapWord *old_start = mr.start();
  HeapWord *old_end = mr.end();

  *start = (HeapWord*)round_to((intptr_t)old_start, page_size());
  *tail = (HeapWord*)round_down((intptr_t)old_end, page_size());
}

void BDCPromotionLAB::flush() {
  assert(_state != flushed, "Attempt to flush PLAB twice");
  assert(top() <= end(), "pointers out of order");

  if(_state == zero_size)
    return;

  //The whole PLABs regions are filled with an unreachable array of type INT.
  int len = collections()->length();
  for(int i = 0; i < len; ++i) {
    PLABRegion *r = collections()->at(i);
    HeapWord *top = r->top();
    HeapWord *region_end = r->end() + filler_header_size;
    typeArrayOop filler_oop = (typeArrayOop)top;
    filler_oop->set_mark(markOopDesc::prototype());
    filler_oop->set_klass(Universe::intArrayKlassObj());
    const size_t array_length =
      pointer_delta(region_end, top) - typeArrayOopDesc::header_size(T_INT);
    assert( (array_length * (HeapWordSize/sizeof(jint))) < (size_t)max_jint,
            "array too big in BDCPromotionLAB");
    filler_oop->set_length((int)(array_length * (HeapWordSize/sizeof(jint))));
#ifdef ASSERT
    // Note that we actually DO NOT want to use the aligned header size!
    HeapWord* elt_words = ((HeapWord*)filler_oop) + typeArrayOopDesc::header_size(T_INT);
    Copy::fill_to_words(elt_words, array_length, 0xDEAABABE);

    // Clear all pointers for the regions
    r->set_bottom(NULL);
    r->set_top(NULL);
    r->set_end(NULL);
#endif
  }

  set_bottom(NULL);
  set_end(NULL);
  set_top(NULL);

  _state = flushed;
}


bool
BDCPromotionLAB::unallocate_object(HeapWord* obj, size_t obj_size) {
  assert(Universe::heap()->is_in(obj), "BDCPromotionLAB : Object is outside of the heap");

  if(contains(obj)) {
    int index = grp_index_contains_obj(obj);
    assert(index != -1, "The collections() was not properly initialized");
    PLABRegion* r = collections()->at(index);
    HeapWord *old_top = r->top();
    HeapWord *new_top = old_top - obj_size;
    r->set_top(new_top);

    assert(r->top() == obj, "object not deallocated");

    if(old_top == top()) {
        set_top(obj);
    }
    return true;
  }
  return false;
}

MemRegion
BDCPromotionLAB::used_region(BDARegion type) {
  int i = collections()->find(&type, PLABRegion::equals);

  if (i == -1)
    return MemRegion();

  PLABRegion *r = collections()->at(i);
  return MemRegion(r->bottom(), r->top());
}

size_t
BDCPromotionLAB::capacity(BDARegion type) const {
  int i = collections()->find(&type, PLABRegion::equals);

  if(i == -1)
    return 0;

  PLABRegion *r = collections()->at(i);
  return byte_size(r->bottom(), r->end());
}

size_t
BDCPromotionLAB::used(BDARegion type) const {
  int i = collections()->find(&type, PLABRegion::equals);

  // TODO : Throw error!
  if ( i == -1 )
    return 0;

  PLABRegion *r = collections()->at(i);
  return byte_size(r->bottom(), r->top());
}

size_t
BDCPromotionLAB::free(BDARegion type) const {
  int i = collections()->find(&type, PLABRegion::equals);

  // TODO : Throw error!
  if ( i == -1 )
    return 0;

  PLABRegion *r = collections()->at(i);
  return byte_size(r->top(), r->end());
}



/* ----------------- Old Generation PLAB code ------------------- */

void
BDCOldPromotionLAB::flush() {
  assert(_state != flushed, "Attempt to flush PLAB twice");
  assert(top() <= end(), "pointers out of order");

  if(_state == zero_size)
    return;

  // just a dirty way to save all the tops for each mutable space
  GrowableArray<HeapWord*>* arr = new (ResourceObj::C_HEAP, mtGC)GrowableArray<HeapWord*>(0, true);
  for(int i = 0; i < collections()->length(); ++i) {
    PLABRegion *r = collections()->at(i);
    HeapWord *obj = r->top();
    arr->append(obj);
  }

  BDCPromotionLAB::flush();

  assert(_start_array != NULL, "BDCOldpromotionLAB: start_array is NULL");

  for(int k = 0; k < arr->length(); ++k) {
    _start_array->allocate_block(arr->at(k));
  }

  delete arr;
}

HeapWord*
BDCOldPromotionLAB::allocate(size_t size) {

  Thread* thread = Thread::current();
  BDARegion type = thread->alloc_region();

  int i = collections()->find(&type, PLABRegion::equals);

  // default to the no-collection space
  if(i == -1)
    i = 0;

  PLABRegion *r = collections()->at(i);
  HeapWord *obj = r->top();
  HeapWord *new_top = obj + size;
  if(new_top > obj && new_top <= r->end()) {
    r->set_top(new_top);

    assert(is_object_aligned((intptr_t)obj) && is_object_aligned((intptr_t)new_top),
           "checking alignment");

    // finally update the top for the whole region
    if(r->top() > top()) {
      set_top(r->top());
    }

    _start_array->allocate_block(obj);
    return obj;
  }
  else
    return NULL;
}

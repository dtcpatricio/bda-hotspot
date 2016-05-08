#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_INLINE_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_INLINE_HPP

#include "gc_implementation/shared/bdaGlobals.hpp"

template <class T>
void
BDARegion::encode_oop_element(T* p, BDARegion* r)
{
  // FIXME: Experimental
  T heap_oop = oopDesc::load_heap_oop(p);
  if (!oopDesc::is_null(heap_oop)) {
    oop obj           = oopDesc::decode_heap_oop_not_null(heap_oop);
    BDARegion* new_reg;
    BDARegion* obj_reg = oopDesc::load_region_oop(obj);
    if (!BDARegion::is_valid(r)) {
      new_reg = KlassRegionMap::region_start_ptr();
      assert(is_valid(new_reg), "bda region is still badHeapWord");
    } else if (BDARegion::is_element(r->value())) {
      new_reg = r;
    } else {
      new_reg = r + 1;
    }
      // // old_reg = BDARegion::region_start;

      // assert(!is_bad_region(old_reg), "bda region is still badHeapWord");
      // new_reg = old_reg | 0x1;
      // assert(is_element(new_reg), "bda region transformation unsuccessful");
    assert(is_element(new_reg->value()), "bda region transformation unsuccessful");
    oopDesc::store_region_oop(&obj_reg, new_reg);

    // For now don't assert this because of races in the collector.
    // Maybe try a MemFence?
    // assert(obj->region() == new_reg, "just checking");
  }
}

#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_INLINE_HPP

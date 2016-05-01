#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_INLINE_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_INLINE_HPP

#include "gc_implementation/shared/bdaGlobals.hpp"

template <class T>
inline void
BDARegion::encode_oop_element(T* p, bdareg_t r)
{
  T heap_oop = oopDesc::load_heap_oop(p);
  if (!oopDesc::is_null(heap_oop)) {
    oop obj          = oopDesc::decode_heap_oop_not_null(heap_oop);
    bdareg_t new_reg;
    bdareg_t old_reg = r;
    if (BDARegion::is_bad_region(old_reg))
      old_reg = BDARegion::region_start;

    assert(!is_bad_region(old_reg), "bda region is still badHeapWord");
    new_reg = old_reg | 0x1;

    assert(is_element(new_reg), "bda region transformation unsuccessful");
    obj->set_region(new_reg);

    // For now don't assert this because of races in the collector.
    // Maybe try a MemFence?
    // assert(obj->region() == new_reg, "just checking");
  }
}


#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_INLINE_HPP

#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP

#include <stdint.h>
/*
 * BDARegionDesc implments basic functions to mask out region identifiers
 * as also helps assigning them. BDARegionDesc is always passed as a pointer
 * or as a BDARegion (see typedef below). This ensures that it is always the
 * width of a machine pointer (generally, 64bit).
 */
typedef class BDARegionDesc* BDARegion;

class BDARegionDesc {

private:
  uintptr_t value() const { return (uintptr_t)this; }

public:
  enum BDAType {
    other = 1,
    container = 2,
    element = 3
  };

  static const int       region_shift = 1;
  static const uintptr_t no_region = 0x0;
  static const uintptr_t region_start = 0x1;
  static const int       other_mask = 0x1;
  static const uintptr_t container_mask = (1UL << 63) - 1UL;

  BDARegion mask_out_element() const {
    return BDARegion(value() & container_mask);
  }

  BDARegion set_element() {
    return BDARegion(value() | 0x1);
  }

  BDAType get_bda_type() const {
    if(value() & other_mask && value() & container_mask)  {
      return element;
    } else if (value() & other_mask) {
      return other;
    } else {
      return container;
    }
  }

  int space_id() const {
    int id = 0; uintptr_t v = value();
    while(v != region_start) {
      v >>= 1; ++id;
    }
    return id;
  }

  bool is_null_region() const {
    return value() == no_region;
  }

  inline static BDARegion encode_as_element(void* p) { return BDARegion(p)->set_element(); }


  // Print support (nothing for now...)
  void print() {

  }

  // This method is still a bit insane...
  inline const char* toString()
    {
      switch(value())
      {
      case no_region:
        return "[No Region Assigned]";

      default:
        return (char*)value();
      }
    }
};


#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP

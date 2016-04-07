#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP

#include <stdint.h>
// It starts with an enum that tries to define the kinds of collections it
// supports.

// enum BDARegion {
//   no_region = 0x0,
//   region_other = 0x1,
//   region_hashmap = 0x2,
//   region_hashtable = 0x4
// };
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

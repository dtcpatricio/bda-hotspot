#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP

#include <stdint.h>
#include "utilities/globalDefinitions.hpp"
/*
 * BDARegionDesc implments basic functions to mask out region identifiers
 * as also helps assigning them. BDARegionDesc is always passed as a pointer
 * or as a BDARegion (see typedef below). This ensures that it is always the
 * width of a machine pointer (generally, 64bit).
 */
//typedef class BDARegionDesc* BDARegion;
#ifdef _LP64
typedef uint64_t bdareg_t;
#else
typedef uint32_t bdareg_t;
#endif

class BDARegion {

private:
  bdareg_t _value;

public:

  bdareg_t value() const { return _value; }
  
  enum BDAType {
    other = 1,
    container = 2,
    element = 3
  };

  static const int       region_shift = 1;
  static const bdareg_t  no_region = 0x0;
  static const bdareg_t  region_start = 0x1;
  static const int       other_mask = 0x1;
  static const uintptr_t container_mask = (1UL << 63) - 2UL;
  static const uint      badheap_mask = (1L << 32) - 1L;

  BDARegion() { _value = no_region; }
  BDARegion(bdareg_t v) { _value = v; }
  BDARegion(const BDARegion& region) { _value = region.value(); }

  bool operator==(const BDARegion& comp)  {
    return value() == comp.value();
  }

  
  
  BDARegion mask_out_element() const {
    return BDARegion(value() & container_mask);
  }
  static bdareg_t mask_out_element(bdareg_t value) {
    // We can't mask-out a region_start because it'd turn it into a no-region
    if(value != region_start) return (value & container_mask);
    return region_start;
  }


  void set_element() {
    _value = _value | 0x1;
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
    int id = 0; bdareg_t v = value();
    while(v != region_start) {
      v >>= 1; ++id;
    }
    return id;
  }

  bool is_null_region() const {
    return value() == no_region;
  }
  // bool is_bad_region() const {
  //   return (value() & badheap_mask) == badHeapWord;
  // }

  static bool is_element(bdareg_t v) { return (v & ~container_mask) == region_start; }
  static bool is_bad_region(bdareg_t v) { return (v & badheap_mask) == badHeapWord; }

  // Print support (nothing for now...)
  void print() {

  }

  template <class T> static inline void encode_oop_element(T* p, bdareg_t r);
  
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

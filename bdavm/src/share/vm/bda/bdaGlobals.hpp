#ifndef SHARE_VM_BDA_GLOBALS_HPP
#define SHARE_VM_BDA_GLOBALS_HPP

# include <stdint.h>
# include "utilities/macros.hpp"
# include "memory/allocation.hpp"

typedef uint32_t bdareg_t;

typedef struct container {
  HeapWord* _top;
  HeapWord* _start;
  HeapWord* _end;
  struct container * _next_segment; // For partition of containers into small segments.
  struct container * _prev_segment; // Ease the iteration and removal of segments
  struct container * _next; // For iteration of containers in mutableSpaces
  struct container * _previous; // Serves both the container segments and the double link list
  DEBUG_ONLY(char   _space_id;)
} container_t;

typedef struct container_helper {
  HeapWord *    _top;
  container_t * _container;
} container_helper_t;

/*
 * BDARegion is a wrapper to a bdareg_t value. What it does is provide
 * methods to query the value and interpret the response, i.e., toString(),
 * etc.
 */
class BDARegion : public CHeapObj<mtGC> {

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

  // This two values are for faster access to the limits of the
  // KlassRegionMap::_region_data array and to avoid circular dependencies (and their
  // fixes) for some methods in this class.
  static BDARegion*      region_start_addr;
  static BDARegion*      region_end_addr;


  BDARegion() { _value = no_region; }
  BDARegion(bdareg_t v) { _value = v; }
  BDARegion(const BDARegion& region) { _value = region.value(); }

  static void set_region_start(BDARegion* ptr) { region_start_addr = ptr; }
  static void set_region_end(BDARegion* ptr) { region_end_addr = ptr; }

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
  static BDARegion* mask_out_element_ptr(BDARegion* v);


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
    return log2_intptr((intptr_t)value()) + 1;
  }
  static int space_id(BDARegion* v) {
    if(is_bad_region_ptr(v)) return 0;
    return log2_intptr((intptr_t)v->value()) + 1;
  }

  bool is_null_region() const {
    return value() == no_region;
  }

  static bool is_element(bdareg_t v);
  static bool is_bad_region(bdareg_t v);
  static bool is_bad_region_ptr(BDARegion* ptr);

  static bool is_valid(BDARegion* v) {
    return !is_bad_region_ptr(v) && v != region_start_addr;
  }

  // Print support (nothing for now...)
  void print() {

  }

  template <class T> static void encode_oop_element(T* p, BDARegion* r);

  // This method is still a bit insane...
  inline const char* toString()
    {
      switch(value())
      {
      case other:
        return "General Object";
        break;

      case container:
        return "Container Object";
        break;

      case element:
        return "Element Object";
        break;

      default:
        return "[No Region Assigned]";
      }
    }
};

#endif // SHARE_VM_GLOBALS_HPP

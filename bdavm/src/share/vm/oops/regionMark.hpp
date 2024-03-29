#ifndef SHARE_VM_OOPS_REGIONMARK_HPP
#define SHARE_VM_OOPS_REGIONMARK_HPP

#include "oops/oop.hpp"
#include "gc_implementation/shared/bdaGlobals.hpp"

// This class implements a word that contains information to where the object (oop)
// is allocated in the Big Data allocators (bdcMutableSpace regions).

// FIXME: TRYING TO DEPRECATE THIS CLASS

class regionMarkDesc VALUE_OBJ_CLASS_SPEC {

private:
  // Conversion
  uintptr_t value() const { return (uintptr_t)this; }

public:
  // Constants used for shifting the word to figure out the region
  // to where it allocs
  // FIXME: Unsure at the moment
  enum {
    region_shift = 0,
    region_bits = 32,
    element_bit = 0x1
  };

  enum {
    region_mask          = right_n_bits(region_bits),
    region_mask_in_place = region_mask << region_shift
  };

  // not in use, currently
  enum {
    container_value = 1,
    element_value = 2,
    other_value = 3
  };

  regionMark clear_region_bits() { return regionMark(value() & ~region_mask_in_place); }

  // Not in use, currently ----------------
  bool is_container_oop() const {
    return mask_bits(value(), right_n_bits(region_bits)) == container_value;
  }

  bool is_element_oop() const {
    return mask_bits(value(), right_n_bits(region_bits)) == element_value;
  }

  regionMark set_container() {
    return regionMark((value() & ~region_mask_in_place) | container_value);
  }

  regionMark set_element() {
    return regionMark((value() & ~region_mask_in_place) | element_value);
  }
  // ---------------------------------------

  bool is_noregion_oop() const {
    return mask_bits(value(), right_n_bits(region_bits)) == BDARegion::no_region;
  }

  regionMark set_noregion() const {
    return regionMark((value() & ~region_mask_in_place) | BDARegion::no_region);
  }

  bool is_other_oop() const {
    return mask_bits(value(), right_n_bits(region_bits)) == BDARegion::region_start;
  }

  regionMark set_other() {
    return regionMark((value() & ~region_mask_in_place) | BDARegion::region_start);
  }

  inline regionMark encode_element() { return regionMark(value() | element_bit); }

  // inline statics in order to construct this
  inline static regionMark encode_pointer_as_container(void* p) { return regionMark(p)->set_container(); }

  inline static regionMark encode_pointer_as_element(void* p) { return regionMark(p)->set_element(); }
    
  inline static regionMark encode_pointer_as_other(void* p) { return regionMark(p)->set_other(); }

  inline static regionMark encode_pointer_as_noregion(void* p) { return regionMark(p)->set_noregion(); }

  inline static regionMark encode_pointer_as_region(BDARegion r, void* p) {
    if(r == BDARegion(BDARegion::region_start))
      return encode_pointer_as_other(p);
    else
      return encode_pointer_as_noregion(p);
  }


  // Templates for the iteration macros
  template <class T> static inline void encode_as_element(T* p, regionMark r);

  
  // general inlines
  // FIXME: Unsure at the moment
  inline void* clear_pointer() { return clear_region_bits(); } // it can recover the address of an object

  inline BDARegion decode_pointer() { return (BDARegion)value(); }
  
};

#endif // SHARE_VM_OOPS_REGIONMARK_HPP

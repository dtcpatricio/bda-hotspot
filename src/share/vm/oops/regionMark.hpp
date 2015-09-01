#ifndef SHARE_VM_OOPS_REGIONMARK_HPP
#define SHARE_VM_OOPS_REGIONMARK_HPP

#include "oops/oop.hpp"

// This class implements a word that contains information to where the object (oop)
// is allocated in the Big Data allocators (bdcMutableSpace regions).
// It is implemented as a markOopDesc and, in the future, it could be integrated
// into it through the unused bits (see markOop.hpp).

class regionMarkDesc : public oopDesc {

private:
  // Conversion
  uintptr_t value() const { return (uintptr_t)this; }

public:
  // Constants used for shifting the word to figure out the region
  // to where it allocs
  enum {
    region_bits = 2
  };

  enum {
    region_shift = 0
  };

  enum {
    region_mask          = right_n_bits(region_bits),
    region_mask_in_place = region_mark << region_shift
  }

  enum {
    container_value = 1,
    element_value = 2,
    other_value = 3
  };

  regionMark clear_region_bits() { return (value() & ~region_mask_in_place); }

  bool is_container_oop() const {
    return mask_bits(value(), right_n_bits(region_bits)) == container_value;
  }

  bool is_element_oop() const {
    return mask_bits(value(), right_n_bits(region_bits)) == element_value;
  }

  bool is_other_oop() const {
    return mask_bits(value(), right_n_bits(region_bits)) == other_value;
  }

  void set_container() {
    return regionMark((value() & ~region_mask_in_place) | container_value);
  }

  void set_element() {
    return regionMark((value() & ~region_mask_in_place) | element_value);
  }

  void set_other() {
    return regionMark((value() & ~region_mask_in_place) | other_value);
  }

  // inline statics in order to construct this
  inline static regionMark encode_pointer_as_container(void* p) { return regionMark(p)->set_container(); }

  inline static regionMark encode_pointer_as_element(void* p) { return regionMark(p)->set_element(); }

  inline static regionMark encode_pointer_as_other(void* p) { return regionMark(p)->set_other(); }

  // general inlines
  inline void* decode_pointer() { return clear_region_bits(); } // it can recover the addres of an object
}

#endif // SHARE_VM_OOPS_REGIONMARK_HPP

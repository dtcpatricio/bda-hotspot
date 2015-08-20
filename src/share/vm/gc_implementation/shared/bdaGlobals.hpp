#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP

// It starts with an enum that tries to define the kinds of collections it
// supports.

enum BDACollectionType {
  coll_type_none = 0x1,
  coll_type_hashmap = 0x2,
  coll_type_hashset = 0x4
};

#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP

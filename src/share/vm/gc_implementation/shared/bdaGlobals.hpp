#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP

// It starts with an enum that tries to define the kinds of collections it
// supports.

enum BDARegion {
  no_region = 0x0,
  region_other = 0x1,
  region_hashmap = 0x2,
  region_hashtable = 0x4
};

#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP

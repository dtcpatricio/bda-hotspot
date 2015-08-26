#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP

// It starts with an enum that tries to define the kinds of collections it
// supports.

enum BDARegion {
  region_other = 0x1,
  region_hashmap = 0x2
};

#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP

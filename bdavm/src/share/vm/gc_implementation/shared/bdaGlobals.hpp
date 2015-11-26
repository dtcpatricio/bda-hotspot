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

inline const char* toString(BDARegion r)
{
  switch(r)
  {
  case no_region:
    return "[No Region Assigned]";

  case region_other:
    return "[OTHER]";

  case region_hashmap:
    return "[HASHMAP]";

  case region_hashtable:
    return "[HASHTABLE]";

  default:
    return "[NO REGION]";
  }
}

#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_BDAGLOBALS_HPP

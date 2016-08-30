#include "bda/bdaGlobals.hpp"
#include "oops/klassRegionMap.hpp"

BDARegion* BDARegion::region_start_addr = NULL;
BDARegion* BDARegion::region_end_addr = NULL;

BDARegion*
BDARegion::mask_out_element_ptr(BDARegion* v)
{
  if(is_bad_region_ptr(v)) return KlassRegionMap::region_start_ptr();
  int idx = log2_intptr((intptr_t)v->value());
  if(is_odd((intptr_t)idx) && idx > 2)
    return v - 1;
  return v;
}

bool
BDARegion::is_element(bdareg_t v) {
  return (v & ~container_mask) == region_start;
}
bool
BDARegion::is_bad_region(bdareg_t v)
{
  return (v & badheap_mask) == badHeapWord || (v == 0);
}
bool
BDARegion::is_bad_region_ptr(BDARegion* ptr)
{
  return (ptr < region_start_addr) || (ptr > region_end_addr);
}

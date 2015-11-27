#include "oops/klassRegionMap.hpp"

#include <cmath>

KlassRegionMap::KlassRegionMap(int table_size) {
  _region_table = new (ResourceObj::C_HEAP, mtGC) GrowableArray<char>(table_size, true);
  for(int i = 0; i < table_size; ++i) {
    _region_table->push((char)no_region);
  }
}

KlassRegionMap::~KlassRegionMap() {
  delete _region_table;
}

int
KlassRegionMap::compute_hash(intptr_t k) {
  // Following Knuths suggestion
  double a = (sqrt(5) - 1) / 2;
  uint w = 8 << LogHeapWordSize; // 64 or 32
  uint seed = floor(pow(2,a) * a);
  uint mult = k * seed;
  uint n_low_bits = 24;
  uint hash = mult >> (w - n_low_bits);

  // uint low_bits = mask_bits(k, right_n_bits(n_low_bits));
  // uint mix = floor(seed * (1 << n_low_bits));
  // uint mult = low_bits * mix;
  // uint hash = mult >> (24 - 16);
  return hash;
}

BDARegion
KlassRegionMap::region_for_klass(Klass* k) {
  uint hash = compute_hash((intptr_t)k);
  int index = indexof(hash);
  if(table()->at(index) == (char)no_region) {
    BDARegion region = k->is_subtype_for_bda();
    (table()->at(index)) = (char)region;
    return region;
  } else {
    return (BDARegion)(table()->at(index));
  }
}

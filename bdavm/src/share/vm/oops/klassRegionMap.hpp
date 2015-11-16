#ifndef SHARE_VM_OOPS_KLASSREGIONMAP_HPP
#define SHARE_VM_OOPS_KLASSREGIONMAP_HPP

#include "oops/oop.hpp"
#include "utilities/growableArray.hpp"

class KlassRegionMap : public CHeapObj<mtGC> {
private:
  GrowableArray<char>* _region_table;

protected:
  int compute_hash(intptr_t k);
  int indexof(int hash) const { return hash % table()->length(); };
  GrowableArray<char>* table() const { return _region_table; }

public:
  KlassRegionMap(int table_size);
  ~KlassRegionMap();

  BDARegion region_for_klass(Klass* k);
};

#endif // SHARE_VM_OOPS_KLASSREGIONMAP_HPP

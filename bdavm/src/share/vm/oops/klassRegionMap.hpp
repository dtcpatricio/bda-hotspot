#ifndef SHARE_VM_OOPS_KLASSREGIONMAP_HPP
#define SHARE_VM_OOPS_KLASSREGIONMAP_HPP

#include "gc_implementation/shared/bdaGlobals.hpp"
#include "utilities/hashtable.hpp"
#include "utilities/growableArray.hpp"

/**
 * The Hashtable for the mapping between region identifiers and Klass objects.
 * The Klass objects are identified by their ref
 */

class KlassRegionEntry;

class KlassRegionHashtable : public Hashtable<bdareg_t, mtGC> {

public:
  KlassRegionHashtable(int table_size);

  void add_entry(int index, KlassRegionEntry* entry) {
    Hashtable<bdareg_t, mtGC>::add_entry(index, (HashtableEntry<bdareg_t, mtGC>*)entry);
  }

  void add_entry(Klass* k, bdareg_t region_id);
  bdareg_t get_region(Klass* k);
};

class KlassRegionEntry : public HashtableEntry<bdareg_t, mtGC> {

public:
  KlassRegionEntry* next() const {
    return (KlassRegionEntry*)HashtableEntry<bdareg_t, mtGC>::next();
  }

  KlassRegionEntry** next_addr() {
    return (KlassRegionEntry**)HashtableEntry<bdareg_t, mtGC>::next_addr();
  }
};


/* The concrete class that manages the mapping between region ids and Klass objects.
 * Several fields and methods are static, since we're assuming there's always just one
 * instance of this class.
 */
class KlassRegionMap : public CHeapObj<mtGC> {
private:
  volatile bdareg_t _next_region;
  static KlassRegionHashtable* _region_map;

  // parse the command line string BDAKlasses="..."
  static void parse_from_string(const char* line, void (*parse)(char*));
  static void parse_from_line(char* line);

public:
  static GrowableArray<char*>* _bda_class_names;

  static int number_bdaregions();
  static bool is_bda_type(const char* name);
  static bool equals(void* class_name, char* value) {
    return strcmp((char*)class_name, value) == 0;
  }

  KlassRegionMap();
  ~KlassRegionMap();

  void add_entry(Klass* k);
  // adds an entry for the general object space
  inline void add_other_entry(Klass* k);
  // adds an entry for one of the bda spaces
  inline void add_region_entry(Klass* k);
  // an accessor for the region_map which is used by set_klass(k) to assign a bda space
  static bdareg_t region_for_klass(Klass* k);
};

// Inline definition
inline void
KlassRegionMap::add_other_entry(Klass* k) {
  _region_map->add_entry(k, BDARegion::region_start);
}

inline void
KlassRegionMap::add_region_entry(Klass* k) {
  _next_region <<= BDARegion::region_shift;
  _region_map->add_entry(k, _next_region);
}

#endif // SHARE_VM_OOPS_KLASSREGIONMAP_HPP

#ifndef SHARE_VM_OOPS_KLASSREGIONMAP_HPP
#define SHARE_VM_OOPS_KLASSREGIONMAP_HPP

#include "bda/bdaGlobals.hpp"
#include "utilities/hashtable.hpp"
#include "utilities/growableArray.hpp"

/**
 * The Hashtable for the mapping between region identifiers and Klass objects.
 */

class KlassRegionEntry;

class KlassRegionHashtable : public Hashtable<BDARegion*, mtGC> {

 public:
  KlassRegionHashtable(int table_size);

  void add_entry(int index, KlassRegionEntry* entry) {
    Hashtable<BDARegion*, mtGC>::add_entry(index, (HashtableEntry<BDARegion*, mtGC>*)entry);
  }

  KlassRegionEntry* add_entry(Klass* k, BDARegion* region);
  BDARegion* get_region(Klass* k);
};

class KlassRegionEntry : public HashtableEntry<BDARegion*, mtGC> {

 private:
  Klass* _klass; // actual klass for conflict resolution

 public:
  KlassRegionEntry* next() const {
    return (KlassRegionEntry*)HashtableEntry<BDARegion*, mtGC>::next();
  }

  KlassRegionEntry** next_addr() {
    return (KlassRegionEntry**)HashtableEntry<BDARegion*, mtGC>::next_addr();
  }

  Klass* get_klass()         { return _klass; }
  void   set_klass(Klass* k) { _klass = k; }
};


/* The actual class that manages the mapping between region ids and Klass objects.
 * Several fields and methods are static, since we're assuming there's always just one
 * instance of this class.
 */
class KlassRegionMap : public CHeapObj<mtGC> {

 private:
  static volatile bdareg_t _next_region;
  static BDARegion* _region_data;
  static int        _region_data_sz;
  static KlassRegionHashtable* _region_map;

  // parse the command line string BDAKlasses="..."
  static void parse_from_string(const char* line, void (*parse)(char*));
  static void parse_from_line(char* line);

  // Class that wraps the class names and the ids with they are promoted
  class KlassRegionEl : public CHeapObj<mtGC> {

   private:
    const char*      _klass_name;
    const bdareg_t   _region_id;
   public:
    KlassRegionEl(const char* klass_name, bdareg_t region_id) :
      _klass_name(klass_name), _region_id(region_id) {}

    const char*      klass_name() const { return _klass_name; }
    const bdareg_t   region_id()  const { return _region_id; }
    // routine to find the element with a specific char* value
    static bool equals_name(void* klass_name, KlassRegionEl* value) {
        return strcmp((char*)klass_name, value->klass_name()) == 0;
    }
  };

 public:
  static GrowableArray<KlassRegionEl*>* _bda_class_names;

  static int number_bdaregions();

  KlassRegionMap();
  ~KlassRegionMap();

  // checks if a klass is bda type and returns the appropriate region id
  static BDARegion * is_bda_klass(Klass* k);
  // checks if a klass with "name" is a bda type
  bool     is_bda_type(const char* name);
  // actually gets the region id on where objects familiar to "name" live
  BDARegion * bda_type(Klass* k);
  // adds an antry to the Hashtable of klass_ptr<->bdareg_t
  void add_entry(Klass* k);
  // adds an entry for the general object space
  inline void add_other_entry(Klass* k);
  // adds an entry for one of the bda spaces
  inline void add_region_entry(Klass* k, BDARegion* r);
  // an accessor for the region_map which is used by set_klass(k) to assign a bda space
  static BDARegion* region_for_klass(Klass* k);
  // an accessor for the _region_data array which saves the bdareg_t wrapper
  static BDARegion* region_data() { return _region_data; }
  // an accessor for the elements in the _region_data array
  static BDARegion* region_elem(bdareg_t r) {
    int idx = 0;
    while(_region_data[idx].value() != r)
      ++idx;
    return &_region_data[idx];
  }
  // Fast accessor for no_region and the low_addr of the _region_data array
  static BDARegion* no_region_ptr() { return &_region_data[0]; }
  // Fast accessor for region_start (other's space)
  static BDARegion* region_start_ptr() { return &_region_data[1]; }
  // Fast accessor for the last element in the _region_data array
  static BDARegion* last_region_ptr() { return &_region_data[_region_data_sz - 1]; }

};

// Inline definition
inline void
KlassRegionMap::add_other_entry(Klass* k) {
  _region_map->add_entry(k, region_start_ptr());
}

inline void
KlassRegionMap::add_region_entry(Klass* k, BDARegion* r) {
  _region_map->add_entry(k, r);
}

#endif // SHARE_VM_OOPS_KLASSREGIONMAP_HPP

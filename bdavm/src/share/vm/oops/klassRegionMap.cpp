#include "precompiled.hpp"
#include "oops/klassRegionMap.hpp"
#include "utilities/hashtable.inline.hpp"
#include "memory/allocation.hpp"

// Static definition

GrowableArray<char*>* KlassRegionMap::_bda_class_names = NULL;
KlassRegionHashtable* KlassRegionMap::_region_map = NULL;

// KlassRegion Hashtables definition

KlassRegionHashtable::KlassRegionHashtable(int table_size)
  : Hashtable<BDARegionDesc*, mtGC>(table_size, sizeof(KlassRegionEntry)) {
}

void
KlassRegionHashtable::add_entry(Klass* k, BDARegionDesc* region)
{
  unsigned int compressed_ptr = (unsigned int)Klass::encode_klass_not_null(k);
  KlassRegionEntry* entry = (KlassRegionEntry*)Hashtable<BDARegionDesc*, mtGC>::new_entry(compressed_ptr, region);

  BasicHashtable<mtGC>::add_entry(hash_to_index(compressed_ptr), entry);
}

BDARegion
KlassRegionHashtable::get_region(Klass* k)
{
  // Give the default entry for the general types initialized at vm startup,
  // i.e. before any entry is added when loading classes
  if(!number_of_entries())
    return BDARegion(BDARegionDesc::region_start);

  int i = hash_to_index((unsigned int)Klass::encode_klass_not_null(k));
  KlassRegionEntry* entry = (KlassRegionEntry*)bucket(i);
  // this can happen with vm loaded classes, i.e. those that do not rely on the
  // classFileParser for loading the .class
  if(entry == NULL) {
    this->add_entry(k, BDARegion(BDARegionDesc::region_start));
  }
  return BDARegion(bucket(i)->literal());
}

// KlassRegionMap definition

KlassRegionMap::KlassRegionMap()
{
  _next_region = BDARegionDesc::region_start;
  _bda_class_names = new (ResourceObj::C_HEAP, mtGC)GrowableArray<char*>(0,true);  
  parse_from_string(BDAKlasses, KlassRegionMap::parse_from_line);

  int table_size = _bda_class_names->length() << log2_intptr(sizeof(KlassRegionEntry));
  _region_map = new KlassRegionHashtable(table_size);
}

KlassRegionMap::~KlassRegionMap()
{
  delete _region_map;
}

void
KlassRegionMap::parse_from_string(const char* line, void (*parse_line)(char*))
{
  char buffer[256];
  char delimiter = ',';
  const char* c = line;
  int i = *c++;
  int pos = 0;
  while(i != '\0') {
    if(i == delimiter) {
      buffer[pos++] = '\0';
      parse_line(buffer);
      pos = 0;
    } else {
      buffer[pos++] = i;
    }
    i = *c++;
  }
  // save the last one
  buffer[pos] = '\0';
  parse_line(buffer);
}

void
KlassRegionMap::parse_from_line(char* line)
{
  // Accept dots '.' and slashes '/' but not mixed.
  // Method names are to be accepted in the future
  char delimiter;
  char buffer[64];
  char* str;
  bool delimiter_found = false;
  int i = 0;
  for(char* c = line; *c != '\0'; c++) {
    if(*c == '.' && !delimiter_found) {
      delimiter_found = true;
      delimiter = '.';
    } else if (*c == '/' && !delimiter_found) {
      delimiter_found = true;
      delimiter = '/';
    }
    if(*c == '/' || *c == '.')
      buffer[i++] = delimiter;
    else
      buffer[i++] = *c;
  }
  buffer[i] = '\0';
  str = NEW_C_HEAP_ARRAY(char, i + 1, mtGC);
  strcpy(str, buffer);
  _bda_class_names->push(str);
}

bool
KlassRegionMap::is_bda_type(const char* name)
{
  if(_bda_class_names->find((char*)name, KlassRegionMap::equals) >= 0)
  {
    return true;
  } else {
    return false;
  }
}

void
KlassRegionMap::add_entry(Klass* k)
{
  ResourceMark rm(Thread::current());
  for(int index = 0; index <= (int)k->super_depth(); ++index) {
    if ( index == (int)Klass::primary_super_limit() ) {
      return add_other_entry(k);
    } else if( k->primary_super_of_depth(index) != NULL &&
               is_bda_type(k->primary_super_of_depth(index)->external_name())) {
      // If this is a bda_type (i.e. an interesting class) add to the hashtable
      // as a BDARegion with non-uniform id.
      return add_region_entry(k);
    }
  }
  add_other_entry(k);
}

BDARegion
KlassRegionMap::region_for_klass(Klass* k)
{
  _region_map->get_region(k);
}

int
KlassRegionMap::number_bdaregions()
{
  assert(_bda_class_names != NULL, "KlassRegionMap has not been initialized yet");
  return _bda_class_names->length();
}

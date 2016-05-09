#include "precompiled.hpp"
#include "oops/klassRegionMap.hpp"
#include "utilities/hashtable.inline.hpp"
#include "memory/allocation.hpp"

// Static definition

GrowableArray<KlassRegionMap::KlassRegionEl*>* KlassRegionMap::_bda_class_names = NULL;
KlassRegionHashtable* KlassRegionMap::_region_map = NULL;
BDARegion* KlassRegionMap::_region_data = NULL;
int        KlassRegionMap::_region_data_sz = 0;
volatile bdareg_t KlassRegionMap::_next_region = BDARegion::region_start;

// KlassRegion Hashtables definition

KlassRegionHashtable::KlassRegionHashtable(int table_size)
  : Hashtable<BDARegion*, mtGC>(table_size, sizeof(KlassRegionEntry)) {
}

KlassRegionEntry*
KlassRegionHashtable::add_entry(Klass* k, BDARegion* region)
{
  unsigned int compressed_ptr;
  if(UseCompressedClassPointers) {
    compressed_ptr = (unsigned int)Klass::encode_klass_not_null(k);
  } else {
    compressed_ptr = (uintptr_t)k & 0xFFFFFFFF;
  }
  KlassRegionEntry* entry = (KlassRegionEntry*)Hashtable<BDARegion*, mtGC>::new_entry(compressed_ptr, region); entry->set_klass(k);

  BasicHashtable<mtGC>::add_entry(hash_to_index(compressed_ptr), entry);

  return entry;
}

__attribute__((optimize("O0")))
BDARegion*
KlassRegionHashtable::get_region(Klass* k)
{
  // Give the default entry for the general types initialized at vm startup,
  // i.e. before any entry is added when loading classes
  if(!number_of_entries())
    return KlassRegionMap::no_region_ptr();

  int i;
  if (UseCompressedClassPointers) {
    i = hash_to_index((unsigned int)Klass::encode_klass_not_null(k));
  } else {
    i = hash_to_index((uintptr_t)k & 0xFFFFFFFF);
  }
  KlassRegionEntry* entry = (KlassRegionEntry*)bucket(i);
  // this can happen with vm loaded classes, i.e. those that do not rely on the
  // classFileParser for loading the .class
  if(entry == NULL) {
    entry = this->add_entry(k, KlassRegionMap::region_elem(BDARegion::region_start));
  } else if (entry->next() != NULL) {
    while (entry->get_klass() != k) {
      entry = entry->next();
      if(entry == NULL) {
        entry = this->add_entry(k,
                                KlassRegionMap::region_start_ptr());
      }
      assert(entry != NULL, "klass pointer placed incorrectly");
    }
  }
  return entry->literal();
}

// KlassRegionMap definition

KlassRegionMap::KlassRegionMap()
{
  _next_region = BDARegion::region_start;
  _bda_class_names = new (ResourceObj::C_HEAP, mtGC)GrowableArray<KlassRegionEl*>(0,true);
  parse_from_string(BDAKlasses, KlassRegionMap::parse_from_line);

  // 2 times each 'interesting' class (container and element) and 2 more for
  // region_start and no_region. Initialize it.
  _region_data_sz = 2 * _bda_class_names->length() + 2;
  _region_data = NEW_C_HEAP_ARRAY(BDARegion, _region_data_sz, mtGC);
  _region_data[0] = BDARegion(BDARegion::no_region);
  _region_data[1] = BDARegion(BDARegion::region_start);
  bdareg_t reg = BDARegion::region_start;
  for(int j = 2; j < _region_data_sz; j += 2) {
    reg <<= BDARegion::region_shift;
    // the no_region is occupying idx 0
    int idx = log2_intptr((intptr_t)reg)*2;
    _region_data[idx] = BDARegion(reg); _region_data[idx+1] = BDARegion(reg | 0x1);
  }
  // Set the limits on the BDARegion
  BDARegion::set_region_start(_region_data);
  BDARegion::set_region_end(&_region_data[_region_data_sz - 1]);

  int table_size = 4096;//_bda_class_names->length() << log2_intptr(sizeof(KlassRegionEntry));
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

  // construct the object and push while shifting the _next_region value
  KlassRegionEl* el = new KlassRegionEl(str, _next_region <<= BDARegion::region_shift);
  _bda_class_names->push(el);
}

bool
KlassRegionMap::is_bda_type(const char* name)
{
  if(_bda_class_names->find((char*)name, KlassRegionEl::equals_name) >= 0)
  {
    return true;
  } else {
    return false;
  }
}

__attribute__((optimize("O0")))
BDARegion*
KlassRegionMap::bda_type(const char* name)
{
  int i = _bda_class_names->find((char*)name, KlassRegionEl::equals_name);
  assert(i >= 0 && i < _bda_class_names->length(), "index out of bounds");
  bdareg_t r = _bda_class_names->at(i)->region_id();
  int idx = log2_intptr((intptr_t)r)*2;
  return &_region_data[idx];
}

__attribute__((optimize("O0")))
void
KlassRegionMap::add_entry(Klass* k)
{
  ResourceMark rm(Thread::current());
  for(juint index = 0; index <= k->super_depth(); ++index) {
    if ( index == Klass::primary_super_limit() ) {
      add_other_entry(k);
      return;
    } else if( k->primary_super_of_depth(index) != NULL &&
               is_bda_type(k->primary_super_of_depth(index)->external_name())) {
      // If this is a bda_type (i.e. an interesting class) add to the hashtable
      // as a BDARegion with non-uniform id.
      BDARegion* region = bda_type(k->primary_super_of_depth(index)->external_name());
      add_region_entry(k, region);
      return;
    }
  }
  add_other_entry(k);
}

BDARegion*
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

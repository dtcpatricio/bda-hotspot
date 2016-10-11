#ifndef SHARE_VM_BDA_BDAPARALLELCOMPACT_HPP
#define SHARE_VM_BDA_BDAPARALLELCOMPACT_HPP

#include "utilities/growableArray.hpp"
#include "utilities/globalDefinitions.hpp"
#include "bda/mutableBDASpace.hpp"

/* This file provides multiple auxiliary classes to aid the ParallelCompact GC when
 * adapted to the BDA Spaces. The classes in this file are carefuly introduced in the
 * Parallel Compact structures to avoid unnecessary conditionals and preprocessor
 * flags to hide the various implemetations at compile time.
 */

class BDADataCounters : public CHeapObj<mtGC> {

public:

  class CounterData : public CHeapObj<mtGC>  {

  private:
    size_t _region_id;
    int*   _counters;

  public:

    void initialize(int counter_sz);
    void incr_counter(int id);
    int* counters() { return _counters; }
  };

  BDADataCounters(int counter_sz) : _counter_sz(counter_sz) {}

  CounterData* initialize_counter_data(size_t region_count);
  int most_counts_id(size_t region_idx);

  int counter_size() { return _counter_sz; }
private:
  CounterData* _map;
  int _counter_sz;

};

class BDASummaryMap VALUE_OBJ_CLASS_SPEC {

private:
  int         _length;
  HeapWord*** _target_nexts;
  HeapWord**  _target_ends;

public:

  BDASummaryMap();

  bool initialize(MutableBDASpace* sp);
  void reset();

  void set_end_word(int id, HeapWord* hw);
  void set_next_addr(int id, HeapWord** hw);

  HeapWord** next_addr_at(int id) { return _target_nexts[id]; }
  HeapWord* end_at(int id) { return _target_ends[id]; }
  HeapWord** end_addr_at(int id) { return &(_target_ends[id]); }
};
#endif // SHARE_VM_BDA_BDAPARALLELCOMPACT_HPP

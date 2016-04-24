#include "gc_implementation/parallelScavenge/parallelScavengeHeap.hpp"
#include "gc_implementation/parallelScavenge/bdaParallelCompact.hpp"

typedef BDADataCounters::CounterData CounterData;

CounterData*
BDADataCounters::initialize_counter_data(size_t region_count)
{
  int counter_sz = ((BDCMutableSpace*)ParallelScavengeHeap::old_gen()->object_space())->collections()->length();
  // Would it be easier to use array initialization? That'd require the CounterData
  // to be a ResourceObj...
  _map = NEW_C_HEAP_ARRAY(CounterData, region_count, mtGC);
  
  for(int idx = 0; idx < (int)region_count; ++idx) {
    _map[idx].initialize(counter_sz);
  }
  return _map;
}

void
BDADataCounters::CounterData::initialize(int counter_sz)
{
  _counters = NEW_C_HEAP_ARRAY(int, counter_sz, mtGC);
  memset(_counters, 0, sizeof(int)*counter_sz);
}

void
BDADataCounters::CounterData::incr_counter(int id)
{
  _counters[id] += 1;
}

int
BDADataCounters::most_counts_id(size_t region_idx)
{
  int* counters = _map[region_idx].counters();
  int max = 0, acc = 0;
  for(int idx = 0; idx < _counter_sz; ++idx) {
    if (counters[idx] > acc) {
      max = idx;
      acc = counters[idx];
    }
  }
  return max;
}

/* BDASummaryMap implementation */
BDASummaryMap::BDASummaryMap()
{
  _length = 0;
  _target_nexts = NULL;
  _target_ends = NULL;
}

bool
BDASummaryMap::initialize(BDCMutableSpace* sp)
{
  _length = sp->collections()->length();
  _target_nexts = NEW_C_HEAP_ARRAY(HeapWord**, _length, mtGC);
  _target_ends = NEW_C_HEAP_ARRAY(HeapWord*, _length, mtGC);
  return true;
}

void
BDASummaryMap::reset()
{
  memset(_target_nexts, 0, _length * sizeof(_target_nexts));
  memset(_target_ends, 0, _length * sizeof(_target_ends));
}

void
BDASummaryMap::set_end_word(int id, HeapWord* v)
{
  // adjust the id because the ids for bda spaces are after the to_space id.
  // see SpaceId.
  assert(id < _length, "id is non exists in collections array");
  _target_ends[id] = v;
}

void
BDASummaryMap::set_next_addr(int id, HeapWord** v)
{
  assert(id < _length, "id is non exists in collections array");
  _target_nexts[id] = v;
}

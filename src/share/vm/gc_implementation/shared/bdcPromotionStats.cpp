#include "gc_implementation/shared/bdcPromotionStats.hpp"

BDCPromotionStats::BDCPromotionStats() : _last_copy_type(last_copy_none),
                                         _c_hashmap_node_copies(0),
                                         _c_hashmap_nodeArr_copies(0),
                                         _node_ahead_copies(0)
{
  _lock = new Monitor(Mutex::barrier, "BDC Promotion Lock", true);
}

void
BDCPromotionStats::print_hashmap_stats() {
  if(!_lock->owned_by_self())
    return;

  tty->print_cr("-----------------------------------------------");
  tty->print_cr("Consequent HashMap Node object copies: %u", _c_hashmap_node_copies);
  tty->print_cr("Consequent HashMap Node Array object copies: %u", _c_hashmap_nodeArr_copies);

  _c_hashmap_node_copies = 0;
  _c_hashmap_nodeArr_copies = 0;
}

void
BDCPromotionStats::incr_node_copies() {
  _lock->lock();
  _c_hashmap_node_copies++;
  _lock->unlock();
}

void
BDCPromotionStats::incr_nodeArr_copies() {
  _lock->lock();
  if(_c_hashmap_node_copies > 0)
    print_hashmap_stats();

  _c_hashmap_nodeArr_copies++;
  _lock->unlock();
}

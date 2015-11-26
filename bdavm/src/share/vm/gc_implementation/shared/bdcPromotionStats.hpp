#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_BDCPROMOTIONSTATS_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_BDCPROMOTIONSTATS_HPP

#include "utilities/globalDefinitions.hpp"
#include "utilities/ostream.hpp"
#include "memory/allocation.hpp"
#include "runtime/mutex.hpp"

class BDCPromotionStats : public CHeapObj<mtGC> {

private:

  enum last_copy {
    last_copy_none,
    last_copy_hashmap_node,
    last_copy_hashmap_nodeArr
  };

  Monitor*             _lock;
  last_copy            _last_copy_type;
  uint                 _c_hashmap_node_copies;
  uint                 _c_hashmap_nodeArr_copies;

  uint                 _node_ahead_copies;
public:

  BDCPromotionStats();

  void print_hashmap_stats();
  void incr_node_copies();
  void incr_nodeArr_copies();
};

#endif

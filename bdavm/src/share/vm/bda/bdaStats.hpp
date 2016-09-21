# include "memory/allocation.hpp"

class BDAPromotionStats VALUE_OBJ_CLASS_SPEC {

 private:
  
  int _failed_element_oops_count;

 public:

  BDAPromotionStats() :
    _failed_element_oops_count(0) { }
  
  void failed_element_promotion() { _failed_element_oops_count += 1; }
  
};

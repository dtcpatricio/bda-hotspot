#ifndef SHARE_VM_GC_IMPLEMENTATION_PARALLELSCAVENGE_BDAOLDPROMOTIONLAB_HPP
#define SHARE_VM_GC_IMPLEMENTATION_PARALLELSCAVENGE_BDAOLDPROMOTIONLAB_HPP

#include "gc_implementation/parallelScavenge/psPromotionLAB.hpp"
#include "gc_implementation/shared/bdaGlobals.hpp"
#include "oops/klassRegionMap.hpp"
/* 
 * BDAOldPromotionLAB acts as an array of PSPromotionLAB objects, specially
 * of PSOldPromotionLAB. Thus, the relation between BDAOldPromotionLAB and
 * PSOldPromotionLAB is the same as the one between BDCMutableSpace and MutableSpace.
 * The purpose of this class is to intercept any calls to the old generation labs
 * in order to forward the message to the correct LAB, since this happens only on
 * a BDA GC.
 */
class BDAOldPromotionLAB : public PSOldPromotionLAB {
  friend class VMStructs;

private:

  class LABGroup : public CHeapObj<mtGC> {
    BDARegion*         _type;
    PSOldPromotionLAB* _lab;

  public:
    LABGroup(BDARegion* type, ObjectStartArray* start_array) : _type(type) {
      _lab = new PSOldPromotionLAB(start_array);
    }
    ~LABGroup() {
      delete _lab;
    }

    BDARegion*         type() { return _type; }
    PSOldPromotionLAB* lab() { return _lab; }

    // Comparison function to be used as argument for find in array
    static bool equals(void* region, LABGroup* grp) {
      return *(BDARegion**)region == grp->type();
    }
  };

  

  GrowableArray<LABGroup*>* _bda_labs;

public:
  // The first constructor does not need initialization since it is the default for
  // value objects. The array of LABGroup objects is, therefore, initialized in
  // the set_start_array() method.
  BDAOldPromotionLAB() : PSOldPromotionLAB(NULL) {}
  BDAOldPromotionLAB(ObjectStartArray* start_array);

  GrowableArray<LABGroup*>* labs() { return _bda_labs; }
  
  // Intercepts PSPromotionLAB initialize() but ditches lab values and fetches new ones
  // From BDCMutableSpace.
  void initialize(MemRegion lab);
  // Intercepts the flush() call and calls all flushes for the LABGroups
  void flush();
  // Initializes the LABGroup array and its elements
  void set_start_array(ObjectStartArray* start_array);
  // Takes the call to the specific PSOldPromotionLAB
  HeapWord* allocate(size_t size);
  // Takes the call from PSPromotionLAB
  bool unallocate_object(HeapWord* obj, size_t obj_size);
  
};

#endif // SHARE_VM_GC_IMPLEMENTATION_PARALLELSCAVENGE_BDAOLDPROMOTIONLAB_HPP

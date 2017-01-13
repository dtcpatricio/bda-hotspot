#ifndef SHARE_VM_BDA_BDAOLDPROMOTIONLAB_HPP
#define SHARE_VM_BDA_BDAOLDPROMOTIONLAB_HPP

#include "gc_implementation/parallelScavenge/psPromotionLAB.hpp"
#include "bda/bdaGlobals.hpp"
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

  container_t  _container;

 public:
  // The first constructor does not need initialization since it is the default for
  // value objects.
  BDAOldPromotionLAB() : PSOldPromotionLAB(NULL) {}
  BDAOldPromotionLAB(ObjectStartArray* start_array) : PSOldPromotionLAB(start_array) {}

  void initialize(MemRegion lab, container_t container);
  void flush()
    { PSOldPromotionLAB::flush(); }
  // Call the set_start_array on super class
  void set_start_array(ObjectStartArray* start_array)
    { PSOldPromotionLAB::set_start_array(start_array); }
  HeapWord * allocate(size_t size, container_t container);

  debug_only(virtual bool lab_is_valid(MemRegion lab));
};

#endif // SHARE_VM_GC_IMPLEMENTATION_PARALLELSCAVENGE_BDAOLDPROMOTIONLAB_HPP

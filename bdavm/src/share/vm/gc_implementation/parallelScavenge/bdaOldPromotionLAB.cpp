#include "gc_implementation/parallelScavenge/bdaOldPromotionLAB.hpp"
#include "gc_implementation/parallelScavenge/parallelScavengeHeap.hpp"

// TODO: OR FIXME:
// Remove the PSOldPromotionLAB pointer and use as a value object, it may be simpler
// since it survives only for the duration of the minor GC.
BDAOldPromotionLAB::BDAOldPromotionLAB(ObjectStartArray* start_array) :
  PSOldPromotionLAB(NULL), _next_region(BDARegionDesc::region_start)
{
  set_start_array(start_array);
}

void
BDAOldPromotionLAB::initialize(MemRegion words)
{
  // This is generally called during the reset step of the promotion manager.
  // Here we are ensuring that all labs are reset properly and we avoid the
  // lab_is_valid test failure on the PSPromotionLAB::initialize by using the
  // thread to proxy the bda region we are initializing.
  if(Thread::current()->alloc_region()->is_null_region()) {
    for(int i = 0; i < labs()->length(); ++i) {
      LABGroup* el = labs()->at(i);
      Thread::current()->set_alloc_region(el->type());
      HeapWord* top = ParallelScavengeHeap::old_gen()->object_space()->top_specific(el->type());
      el->lab()->initialize(MemRegion(top - words.word_size(), words.word_size()));
    }
  } else {
    BDARegion type = Thread::current()->alloc_region();
    int i = labs()->find(&type, LABGroup::equals);
    // This means that this object did not go through a set_klass().
    // Generally it means that it could well go into the other bda region.
    // TODO: In the future we could parse the oop in the previous frame and
    // go through  a KlassRegionMap::region_for_klass() call.
    if( i == -1 ) {
      i = 0;
      // lab initialization requires this due to asserts
      Thread::current()->set_alloc_region(BDARegion(BDARegionDesc::region_start));
    }
    PSOldPromotionLAB* lab = labs()->at(i)->lab();
    lab->initialize(words);
  }
  
}

void
BDAOldPromotionLAB::set_start_array(ObjectStartArray* start_array)
{
  PSOldPromotionLAB::set_start_array(start_array);
  int nregions = KlassRegionMap::number_bdaregions();
  _bda_labs = new (ResourceObj::C_HEAP, mtGC)GrowableArray<LABGroup*>(nregions, true);
  for(int i = 0; i < nregions; ++i) {
    _bda_labs->push(new LABGroup(BDARegion(_next_region), start_array));
    _next_region <<= BDARegionDesc::region_shift;
  }
}

void
BDAOldPromotionLAB::flush()
{
  // This is a dummy value to alloca the flushing of all labs or not
  if(Thread::current()->alloc_region()->is_null_region()) {
    for(int i = 0; i < labs()->length(); ++i) {
      LABGroup* el = labs()->at(i);
      el->lab()->flush();
    }
  } else {
    BDARegion type = Thread::current()->alloc_region();
    int i = labs()->find(&type, LABGroup::equals);
    // This means that this object did not go through a set_klass().
    // Generally it means that it could well go into the other bda region.
    // TODO: In the future we could parse the oop in the previous frame and
    // go through  a KlassRegionMap::region_for_klass() call.
    if( i == -1 ) i = 0;
    PSOldPromotionLAB* lab = labs()->at(i)->lab();
    lab->flush();
  }
}

HeapWord*
BDAOldPromotionLAB::allocate(size_t size)
{
  BDARegion type = Thread::current()->alloc_region();
  int i = labs()->find(&type, LABGroup::equals);
  // This means that this object did not go through a set_klass().
  // Generally it means that it could well go into the other bda region.
  // TODO: In the future we could parse the oop in the previous frame and
  // go through  a KlassRegionMap::region_for_klass() call.
  if( i == -1 ) i = 0;
  PSOldPromotionLAB* lab = labs()->at(i)->lab();
  return lab->allocate(size);
}

bool
BDAOldPromotionLAB::unallocate_object(HeapWord* obj, size_t obj_size)
{
  BDARegion type = Thread::current()->alloc_region();
  int i = labs()->find(&type, LABGroup::equals);
  // This means that this object did not go through a set_klass().
  // Generally it means that it could well go into the other bda region.
  // TODO: In the future we could parse the oop in the previous frame and
  // go through  a KlassRegionMap::region_for_klass() call.
  if( i == -1 ) i = 0;
  PSOldPromotionLAB* lab = labs()->at(i)->lab();
  return lab->unallocate_object(obj, obj_size);
}

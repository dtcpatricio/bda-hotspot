# include "bda/bdaScavenge.hpp"

//
// T* is an oop* or a narrowOop*.
// void* is a general pointer which will vary between BDARegion* for bdaroots and
// container_t* for bdarefs.
// 
template<class T, bool promote_immediately> inline void
BDAScavenge::copy_and_push_safe_barrier(PSPromotionManager* pm,
                                        T * p,
                                        void * r,
                                        RefQueue::RefType rt)
{
  // assert(PSScavenge::should_scavenge(p), "revisiting object?");
  oop new_obj;
  oop o = oopDesc::load_decode_heap_oop_not_null(p);
  if (PSScavenge::should_scavenge(p)) {
    
    new_obj = o->is_forwarded()
      ? o->forwardee()
      : pm->copy_bdaref_to_survivor_space<promote_immediately>(o, r, rt);
  } else {
    new_obj = o;
  }
  
  oopDesc::encode_store_heap_oop_not_null(p, new_obj);

  if ((!PSScavenge::is_obj_in_young((HeapWord*)p)) &&
     Universe::heap()->is_in_reserved(p)) {
    if (PSScavenge::is_obj_in_young(new_obj)) {
      PSScavenge::card_table()->inline_write_ref_field_gc(p, new_obj);
    }
  }
}

#ifndef SHARE_VM_BDA_BDASCAVENGE_HPP
#define SHARE_VM_BDA_BDASCAVENGE_HPP

# include "memory/iterator.hpp"
# include "gc_implementation/parallelScavenge/psScavenge.hpp"
# include "gc_implementation/parallelScavenge/psPromotionManager.hpp"

//
// Contrary to psScavenge.hpp, bdaScavenge.hpp is not an adaptation of it, but rather an
// adaption of psScavenge.inline.hpp.
//
class OopClosure;
class PSPromotionManager;

class BDAScavenge: public PSScavenge {

 public:
  template <class T, bool promote_immediately>
  inline static void copy_and_push_safe_barrier(PSPromotionManager * pm, T * p,
                                                void * r, RefQueue::RefType rt);
};

template<bool promote_immediately>
class BDARootsClosure : public OopClosure {
 private:
  PSPromotionManager * _pm;

 protected:
  template <class T> void do_oop_work(T * p) {
    if (PSScavenge::should_scavenge(p)) {
      BDAScavenge::copy_and_push_safe_barrier<T, promote_immediately>(_pm, p);
    }
  }

 public:
  BDARootsClosure(PSPromotionManager * pm) : _pm(pm) { }
  void do_oop(oop * p)       { BDARootsClosure::do_oop_work(p); }
  void do_oop(narrowOop * p) { BDARootsClosure::do_oop_work(p); }
};

typedef BDARootsClosure</*promote_immediately=*/true> BDARootRefClosure;

#endif

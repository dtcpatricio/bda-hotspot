#ifndef SHARE_VM_BDA_BDATASKS_HPP
#define SHARE_VM_BDA_BDATASKS_HPP

# include "bda/refqueue.hpp"
# include "bda/mutableBDASpace.hpp"
# include "gc_implementation/parallelScavenge/psOldGen.hpp"
# include "gc_implementation/parallelScavenge/gcTaskManager.hpp"


//
// bdaTasks.hpp defines GCTasks used by the adapted parallelScavenge collector.
//

//
// BDARefRootsTask tries to pop bda-refs from the refqueue and checks if each ref is
// appropriate for promotion.
// 
class BDARefRootsTask : public GCTask {
 private:
  RefQueue * _refqueue;
  PSOldGen * _old_gen;

 public:
  BDARefRootsTask(RefQueue * refqueue,
                  PSOldGen * old_gen) :
    _refqueue(refqueue),
    _old_gen(old_gen) { }

  char * name() { return (char*)"big-data refs roots task"; }

  virtual void do_it(GCTaskManager * manager, uint which);
};

//
// OldToYoungBDARootsTask iterates through the containers of all bda spaces and pushes
// the contents of the oops to the bdaref_stack, for later direct promotion. It uses the
// _gc_current value in each CGRPSpace (see mutableBDASpace.hpp) where it CAS the next value
// and returns the pointer that was read.
//
class OldToYoungBDARootsTask : public GCTask {

 private:
  PSOldGen * _old_gen;
  BDACardTableHelper * _helper;

 public:
  OldToYoungBDARootsTask(PSOldGen * old_gen, BDACardTableHelper * helper) :
    _old_gen(old_gen), _helper(helper) { }

  char * name() { return (char*)"big-data old to young roots task"; }

  virtual void do_it(GCTaskManager * manager, uint which);  
};


//
// The RefStack is a stack whose elements are to group bda container refs
// (i.e. elements of a collection) with the container they belong to. BDARefTask is that element.
// It works much as a StarTask (see taskqueue.hpp) but it groups the oop with the container it
// belongs to in the bda-spaces to inform the collector where the oop should be promoted.
//
class BDARefTask {

  void *        _holder;
  container_t * _container;

  enum { COMPRESSED_OOP_MASK = 1 };
  
 public:
  
  BDARefTask(narrowOop * p, container_t * c) : _container(c) {
    assert(((uintptr_t)p & COMPRESSED_OOP_MASK) == 0, "Information loss!");
    _holder = (void*)((uintptr_t)p | COMPRESSED_OOP_MASK);
  }
  BDARefTask(oop * p, container_t * c) : _container(c) {
    assert(((uintptr_t)p & COMPRESSED_OOP_MASK) == 0, "Information loss!");
    _holder = (void*)p;
  }
  BDARefTask()                 { _holder = NULL; _container = NULL; }
  operator oop * ()            { return (oop*)_holder; }
  operator narrowOop * ()      { return (narrowOop*)((uintptr_t)_holder & ~COMPRESSED_OOP_MASK); }
  container_t * container ()   { return _container; }

  bool is_narrow() const {
    return (((uintptr_t)_holder & COMPRESSED_OOP_MASK) != 0);
  }
};

typedef Stack<BDARefTask, mtGC> BDARefStack;

#endif // SHARE_VM_BDA_BDATASKS_HPP

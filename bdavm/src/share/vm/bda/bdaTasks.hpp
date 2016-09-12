#ifndef SHARE_VM_BDA_BDATASKS_HPP
#define SHARE_VM_BDA_BDATASKS_HPP

# include "bda/refqueue.hpp"
# include "bda/mutableBDASpace.hpp"
# include "gc_implementation/parallelScavenge/psOldGen.hpp"
# include "gc_implementation/parallelScavenge/gcTaskManager.hpp"


//
// bdaTasks.hpp defines GCTasks used by the adapted parallelScavenge collector.
// It tries to pop bda-refs from the refqueue and checks if each ref is
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

#endif

#ifndef SHARE_VM_BDA_BDATASKS_HPP
#define SHARE_VM_BDA_BDATASKS_HPP

# include "bda/refqueue.hpp"
# include "bda/mutableBDASpace.inline.hpp"
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
// StealBDARefTask tries to grab tasks in the other gc thread task queues, until there's
// no more BDARefTasks to pop and the scavenger can proceed with the thread's stacks.
//
class StealBDARefTask : public GCTask
{
 private:
  ParallelTaskTerminator * _terminator;

 public:
  StealBDARefTask (ParallelTaskTerminator * t) :
    _terminator(t) { }

  char * name () { return (char*)"steal big-data ref task"; }

  ParallelTaskTerminator * terminator () { return _terminator; }
  
  virtual void do_it (GCTaskManager * manager, uint which);
};

//
// OldToYoungBDARootsTask iterates through the containers of all bda spaces and pushes
// the contents of the oops to the bdaref_stack, for later direct promotion. It uses the
// _gc_current value in each CGRPSpace (see mutableBDASpace.hpp) where it CAS the next value
// and returns the pointer that was read.
//
class OldToYoungBDARootsTask : public GCTask
{

 private:
  PSOldGen * _old_gen;
  uint       _total_workers;

 public:
  OldToYoungBDARootsTask(PSOldGen * old_gen, uint total_workers) :
    _old_gen(old_gen), _total_workers (total_workers) { }

  char * name() { return (char*)"big-data old to young roots task"; }

  virtual void do_it(GCTaskManager * manager, uint which);  
};

//
// OldToYoungNonBDARootsTask works in a similar fashion as OldToYoungRootsTask of psTasks.hpp,
// but instead of blindingly scan the contents of the whole space it uses the bitmap from
// MutableBDASpace to jump container segment regions of the space. It is used to scan the
// objects that do not belong to container segments.
class OldToYoungNonBDARootsTask : public GCTask
{
 private:
  PSOldGen * _old_gen;
  HeapWord * _gen_top;
  uint       _stripe_number;
  uint       _stripe_total;

 public:
  OldToYoungNonBDARootsTask(PSOldGen * old_gen, HeapWord * gen_top,
                            uint stripe_number, uint stripe_total) :
    _old_gen(old_gen), _gen_top(gen_top), _stripe_number(stripe_number), _stripe_total(stripe_total)
    { }

  char * name () { return (char*)"non big-data old young roots task"; }

  virtual void do_it(GCTaskManager * manager, uint which);
};

#endif // SHARE_VM_BDA_BDATASKS_HPP

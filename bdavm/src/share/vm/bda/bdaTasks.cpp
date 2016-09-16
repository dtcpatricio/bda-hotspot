# include "bda/bdaTasks.hpp"
# include "gc_implementation/parallelScavenge/gcTaskManager.hpp"
# include "gc_implementation/parallelScavenge/psPromotionManager.hpp"
# include "gc_implementation/parallelScavenge/psPromotionManager.inline.hpp"

//
// BDARefRootsTask
//
void
BDARefRootsTask::do_it(GCTaskManager * manager, uint which)
{
  PSPromotionManager * pm = PSPromotionManager::gc_thread_promotion_manager(which);
  Ref * r = NULL;
  while(_refqueue->peek() != NULL) {
    r = _refqueue->dequeue();

    // Ugly code ---
    // FIXME: could this be better (and faster) is already implicit the kind of oop?
    if(UseCompressedOops)
      pm->process_dequeued_bdaroot<narrowOop>(r);
    else
      pm->process_dequeued_bdaroot<oop>(r);
    
    pm->drain_bda_stacks();
  }
}

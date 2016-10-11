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
  while((r = _refqueue->dequeue()) != NULL) {
    // Ugly code ---
    // FIXME: could this be better (and faster) is already implicit the kind of oop?
    if(UseCompressedOops)
      pm->process_dequeued_bdaroot<narrowOop>(r);
    else
      pm->process_dequeued_bdaroot<oop>(r);
    
    pm->drain_bda_stacks();
  }
}


//
// OldToYoungBDARootsTask
//
void
OldToYoungBDARootsTask::do_it(GCTaskManager * manager, uint which)
{
  assert (((MutableBDASpace*)_old_gen->object_space())->used_in_words() > 0, "Should not be called if there is no work.");
  assert (_old_gen->object_space() != NULL, "Sanity");

  {
    PSPromotionManager * pm = PSPromotionManager::gc_thread_promotion_manager(which);
    MutableBDASpace * space = (MutableBDASpace*)_old_gen->object_space();
    assert (Universe::heap()->kind() == CollectedHeap::ParallelScavengeHeap, "Sanity");
    CardTableExtension * card_table = (CardTableExtension*)Universe::heap()->barrier_set();

    // Here we iterate through the bda spaces only, thus starting with the first which is in index 1
    // on the array of spaces.
    _helper->prefetch_array();
    for (int spc_id = 1; spc_id < space->spaces()->length(); ++spc_id) {
      MutableBDASpace::CGRPSpace * bda_space = space->spaces()->at(spc_id);
      container_t * c; HeapWord * c_top;
      while ((c = bda_space->cas_get_next_container()) != NULL) {
        do {
          c_top = _helper->top(c);
          if (c_top == NULL) continue;
          assert (c->_start < c_top, "containers are not empty allocated");
          card_table->scavenge_bda_contents_parallel(_old_gen->start_array(),
                                                     c,
                                                     _helper->top(c),
                                                     pm);
          pm->drain_bda_stacks();
        } while ((c = c->_next_segment) != NULL);
      }
    }
  }
}

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
  }
  pm->drain_bda_stacks();
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
    for (int spc_id = 0; spc_id < space->spaces()->length(); ++spc_id) {
      MutableBDASpace::CGRPSpace * bda_space = space->spaces()->at(spc_id);
      container_t * c; HeapWord * c_top;
      while ((c = bda_space->cas_get_next_container()) != NULL) {
        do {
          c_top = _helper->top(c);
          if (c_top == NULL || c->_start == c_top) continue;
          assert (c->_start < c_top, "containers and segments are not empty allocated");
          card_table->scavenge_bda_contents_parallel(_old_gen->start_array(),
                                                     c,
                                                     _helper->top(c),
                                                     pm);
        } while ((c = c->_next_segment) != NULL);
      }
      pm->drain_bda_stacks();
    }
  }
}

//
// OldToYoungNonBDARootsTask
//
void
OldToYoungNonBDARootsTask::do_it(GCTaskManager * manager, uint which)
{
  assert (_old_gen != NULL, "Sanity");

  MutableBDASpace * bda_space      = (MutableBDASpace*)_old_gen->object_space();
  MutableBDASpace::CGRPSpace * grp = bda_space->non_bda_grp();
  MutableSpace *    space          = bda_space->non_bda_space();

  assert (grp != NULL, "Sanity");
  assert (space != NULL, "Sanity");
  assert (space->not_empty(), "Should not be called if there is no work");
  assert (_gen_top <= space->top(), "Wrong top pointer was saved");

  {
    PSPromotionManager * pm = PSPromotionManager::gc_thread_promotion_manager(which);
    assert (Universe::heap()->kind() == CollectedHeap::ParallelScavengeHeap, "Sanity");
    CardTableExtension * card_table = (CardTableExtension*)Universe::heap()->barrier_set();

    HeapWord * bottom = space->bottom();

    while (bottom < _gen_top) {
      HeapWord * const tmp_top = bda_space->get_next_beg_seg(bottom, _gen_top);
      card_table->scavenge_nonbda_contents_parallel(_old_gen->start_array(),
                                                    bottom,
                                                    tmp_top,
                                                    pm,
                                                    _stripe_number,
                                                    _stripe_total);
      HeapWord * const new_bottom = bda_space->get_next_end_seg(tmp_top + 1, _gen_top) + 1;
      assert (pointer_delta(new_bottom, bottom) >= MutableBDASpace::CGRPSpace::segment_sz ||
              new_bottom > _gen_top,
              "incorrect size for the jumped segment");
      bottom = new_bottom;
    }

    // Do the real work
    pm->drain_stacks(false);
  }
}

# include "bda/bdaTasks.hpp"
# include "gc_implementation/parallelScavenge/gcTaskManager.hpp"
# include "gc_implementation/parallelScavenge/psPromotionManager.hpp"
# include "gc_implementation/parallelScavenge/psPromotionManager.inline.hpp"

#ifdef BDA
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
// StealBDARefTask
//
void
StealBDARefTask::do_it (GCTaskManager * manager, uint which)
{
  PSPromotionManager * pm =
    PSPromotionManager::gc_thread_promotion_manager(which);
  pm->drain_bda_stacks();
  guarantee (pm->bda_stacks_empty(), "bda stacks should be empty at this point");

  int random_seed = 17;
  while (true) {
    BDARefTask p;
    if (PSPromotionManager::bda_steal_depth (which, &random_seed, p)) {
      if (UseCompressedOops)
        pm->process_popped_bdaref_depth<narrowOop>(p);
      else
        pm->process_popped_bdaref_depth<oop>(p);
      pm->drain_bda_stacks();
    } else {
      if (terminator()->offer_termination()) {
        break;
      }
    }
  }
  guarantee(pm->bda_stacks_empty(), "stacks should be empty at this point");
}

//
// OldToYoungBDARootsTask
//
void
OldToYoungBDARootsTask::do_it(GCTaskManager * manager, uint which)
{
  assert (((MutableBDASpace*)_old_gen->object_space())->used_in_words() > 0, "Should not be called if there is no work.");
  assert (_old_gen->object_space() != NULL, "Sanity");
  assert (_order_number < ParallelGCThreads, "Sanity");

  {
    PSPromotionManager * pm = PSPromotionManager::gc_thread_promotion_manager(which);
    MutableBDASpace * space = (MutableBDASpace*)_old_gen->object_space();
    assert (Universe::heap()->kind() == CollectedHeap::ParallelScavengeHeap, "Sanity");
    CardTableExtension * card_table = (CardTableExtension*)Universe::heap()->barrier_set();

    // Here we iterate through the bda spaces only, thus starting with the first which is in index 1
    // on the array of spaces.
    for (int spc_id = 0; spc_id < space->spaces()->length(); ++spc_id) {
      MutableBDASpace::CGRPSpace * bda_space = space->spaces()->at(spc_id);
      // This value is preferable to container_count > 0, because containers may have been alloc'd.
      if (bda_space->last_before_gc() == NULL) continue;
      container_t c = bda_space->get_previous_n_segment(bda_space->last_before_gc(), _order_number);
      HeapWord * c_top; int j = 0;
      while (c != NULL) {
#ifdef ASSERT
        if (BDAPrintOldToYoungTasks && Verbose) {
          gclog_or_tty->print_cr ("[%s] thread " INT32_FORMAT " scanning container "
                                  PTR_FORMAT " with contents [" PTR_FORMAT ", " PTR_FORMAT ", "
                                  PTR_FORMAT ") saved top " PTR_FORMAT " scanned by " INT32_FORMAT,
                                  name(), which, (intptr_t)c,
                                  (intptr_t)c->_start, (intptr_t)c->_top,
                                  (intptr_t)c->_end, (intptr_t)c->_saved_top,
                                  c->_scanned_flag);
        }
#endif // ASSERT
        if (c->_saved_top == NULL || c->_start == c->_saved_top) {
#ifdef ASSERT
          if (BDAPrintOldToYoungTasks && Verbose) {
            gclog_or_tty->print_cr ("[%s] thread " INT32_FORMAT " skipped container "
                                    PTR_FORMAT,
                                    name(), which, (intptr_t)c);
          }
#endif // ASSERT
          c = bda_space->get_previous_n_segment(c, _batch_width);
          continue;
        }

#ifdef ASSERT
        assert (c->_scanned_flag == -1, "this segment was already scanned.");
        c->_scanned_flag = (int)_order_number;
#endif // ASSERT
        assert (c->_start < c->_saved_top, "container or segment is empty allocated");
        card_table->scavenge_bda_contents_parallel(_old_gen->start_array(),
                                                   c,
                                                   c->_saved_top,
                                                   pm);
        c = bda_space->get_previous_n_segment(c, _batch_width);
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
      // No need to scan empty space
      if (bottom != tmp_top) {
        card_table->scavenge_nonbda_contents_parallel(_old_gen->start_array(),
                                                      bottom,
                                                      tmp_top,
                                                      pm,
                                                      _stripe_number,
                                                      _stripe_total);
      }
      // We've reached the end of the space
      if (tmp_top == _gen_top) break;
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

#endif // BDA

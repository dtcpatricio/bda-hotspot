/*
 * Copyright (c) 2002, 2014, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_VM_GC_IMPLEMENTATION_PARALLELSCAVENGE_PSPROMOTIONMANAGER_INLINE_HPP
#define SHARE_VM_GC_IMPLEMENTATION_PARALLELSCAVENGE_PSPROMOTIONMANAGER_INLINE_HPP

#include "gc_implementation/parallelScavenge/psOldGen.hpp"
#include "gc_implementation/parallelScavenge/psPromotionManager.hpp"
#include "gc_implementation/parallelScavenge/psPromotionLAB.inline.hpp"
#include "gc_implementation/parallelScavenge/psScavenge.hpp"
#include "oops/oop.psgc.inline.hpp"

#ifdef BDA
# include "bda/bdaScavenge.inline.hpp"
#endif

inline PSPromotionManager* PSPromotionManager::manager_array(int index) {
  assert(_manager_array != NULL, "access of NULL manager_array");
  assert(index >= 0 && index <= (int)ParallelGCThreads, "out of range manager_array access");
  return &_manager_array[index];
}

template <class T>
inline void PSPromotionManager::claim_or_forward_internal_depth(T* p) {
  if (p != NULL) { // XXX: error if p != NULL here
    oop o = oopDesc::load_decode_heap_oop_not_null(p);
    if (o->is_forwarded()) {
      o = o->forwardee();
      // Card mark
      if (PSScavenge::is_obj_in_young(o)) {
        PSScavenge::card_table()->inline_write_ref_field_gc(p, o);
      }
      oopDesc::encode_store_heap_oop_not_null(p, o);
    } else {
      push_depth(p);
    }
  }
}

template <class T>
inline void PSPromotionManager::claim_or_forward_depth(T* p) {
  assert(PSScavenge::should_scavenge(p, true), "revisiting object?");
  assert(Universe::heap()->kind() == CollectedHeap::ParallelScavengeHeap,
         "Sanity");
  assert(Universe::heap()->is_in(p), "pointer outside heap");

  claim_or_forward_internal_depth(p);
}

//
// This method is pretty bulky. It would be nice to split it up
// into smaller submethods, but we need to be careful not to hurt
// performance.
//
template<bool promote_immediately>
oop PSPromotionManager::copy_to_survivor_space(oop o) {
  assert(PSScavenge::should_scavenge(&o), "Sanity");

  oop new_obj = NULL;

  // NOTE! We must be very careful with any methods that access the mark
  // in o. There may be multiple threads racing on it, and it may be forwarded
  // at any time. Do not use oop methods for accessing the mark!
  markOop test_mark = o->mark();

  // The same test as "o->is_forwarded()"
  if (!test_mark->is_marked()) {
    bool new_obj_is_tenured = false;
    size_t new_obj_size = o->size();

    if (!promote_immediately) {
      // Find the objects age, MT safe.
      uint age = (test_mark->has_displaced_mark_helper() /* o->has_displaced_mark() */) ?
        test_mark->displaced_mark_helper()->age() : test_mark->age();

      // Try allocating obj in to-space (unless too old)
      if (age < PSScavenge::tenuring_threshold()) {
        new_obj = (oop) _young_lab.allocate(new_obj_size);
        if (new_obj == NULL && !_young_gen_is_full) {
          // Do we allocate directly, or flush and refill?
          if (new_obj_size > (YoungPLABSize / 2)) {
            // Allocate this object directly
            new_obj = (oop)young_space()->cas_allocate(new_obj_size);
          } else {
            // Flush and fill
            _young_lab.flush();

            HeapWord* lab_base = young_space()->cas_allocate(YoungPLABSize);
            if (lab_base != NULL) {
              _young_lab.initialize(MemRegion(lab_base, YoungPLABSize));
              // Try the young lab allocation again.
              new_obj = (oop) _young_lab.allocate(new_obj_size);
            } else {
              _young_gen_is_full = true;
            }
          }
        }
      }
    }

    // Otherwise try allocating obj tenured
    if (new_obj == NULL) {
#ifndef PRODUCT
      if (Universe::heap()->promotion_should_fail()) {
        return oop_promotion_failed(o, test_mark);
      }
#endif  // #ifndef PRODUCT


      new_obj = (oop) _old_lab.allocate(new_obj_size);

      new_obj_is_tenured = true;

      if (new_obj == NULL) {
        if (!_old_gen_is_full) {
          // Do we allocate directly, or flush and refill?
          if (new_obj_size > (OldPLABSize / 2)) {
            // Allocate this object directly
            new_obj = (oop)old_gen()->cas_allocate(new_obj_size);
          } else {
            // Flush and fill
            HeapWord* lab_base = old_gen()->cas_allocate(OldPLABSize);
            if(lab_base != NULL) {
#ifdef ASSERT
              // Delay the initialization of the promotion lab (plab).
              // This exposes uninitialized plabs to card table processing.
              if (GCWorkerDelayMillis > 0) {
                os::sleep(Thread::current(), GCWorkerDelayMillis, false);
              }
#endif
              _old_lab.flush();
              _old_lab.initialize(MemRegion(lab_base, OldPLABSize));
              // And try the old lab allocation again.
              new_obj = (oop) _old_lab.allocate(new_obj_size);
            }
          }
        }

        // This is the promotion failed test, and code handling.
        // The code belongs here for two reasons. It is slightly
        // different than the code below, and cannot share the
        // CAS testing code. Keeping the code here also minimizes
        // the impact on the common case fast path code.

        if (new_obj == NULL) {
          _old_gen_is_full = true;
          return oop_promotion_failed(o, test_mark);
        }
      }
    }

    assert(new_obj != NULL, "allocation should have succeeded");

    // Copy obj
    Copy::aligned_disjoint_words((HeapWord*)o, (HeapWord*)new_obj, new_obj_size);

    // Now we have to CAS in the header.
    if (o->cas_forward_to(new_obj, test_mark)) {
      // We won any races, we "own" this object.
      assert(new_obj == o->forwardee(), "Sanity");

      // Increment age if obj still in new generation. Now that
      // we're dealing with a markOop that cannot change, it is
      // okay to use the non mt safe oop methods.
      if (!new_obj_is_tenured) {
        new_obj->incr_age();
        assert(young_space()->contains(new_obj), "Attempt to push non-promoted obj");
      }

      // Do the size comparison first with new_obj_size, which we
      // already have. Hopefully, only a few objects are larger than
      // _min_array_size_for_chunking, and most of them will be arrays.
      // So, the is->objArray() test would be very infrequent.
      if (new_obj_size > _min_array_size_for_chunking &&
          new_obj->is_objArray() &&
          PSChunkLargeArrays) {
        // we'll chunk it
        oop* const masked_o = mask_chunked_array_oop(o);
        push_depth(masked_o);
        TASKQUEUE_STATS_ONLY(++_arrays_chunked; ++_masked_pushes);
      } else {
        // we'll just push its contents
        new_obj->push_contents(this);
      }
    }  else {
      // We lost, someone else "owns" this object
      guarantee(o->is_forwarded(), "Object must be forwarded if the cas failed.");

      // Try to deallocate the space.  If it was directly allocated we cannot
      // deallocate it, so we have to test.  If the deallocation fails,
      // overwrite with a filler object.
      if (new_obj_is_tenured) {
        if (!_old_lab.unallocate_object((HeapWord*) new_obj, new_obj_size)) {
          CollectedHeap::fill_with_object((HeapWord*) new_obj, new_obj_size);
        }
      } else if (!_young_lab.unallocate_object((HeapWord*) new_obj, new_obj_size)) {
        CollectedHeap::fill_with_object((HeapWord*) new_obj, new_obj_size);
      }

      // don't update this before the unallocation!
      new_obj = o->forwardee();
    }
  } else {
    assert(o->is_forwarded(), "Sanity");
    new_obj = o->forwardee();
  }

#ifndef PRODUCT
  // This code must come after the CAS test, or it will print incorrect
  // information.
  if (TraceScavenge) {
    gclog_or_tty->print_cr("{%s %s " PTR_FORMAT " -> " PTR_FORMAT " (%d)}",
                           PSScavenge::should_scavenge(&new_obj) ? "copying" : "tenuring",
                           new_obj->klass()->internal_name(), p2i((void *)o), p2i((void *)new_obj), new_obj->size());
  }
#endif

  return new_obj;
}
#ifdef BDA
template <class T> inline void
PSPromotionManager::claim_or_forward_bdaref(T * p, container_t ct)
{
  assert(PSScavenge::should_scavenge(p), "revisiting object?");
  assert(Universe::heap()->kind() == CollectedHeap::ParallelScavengeHeap,
         "Sanity");
  assert(Universe::heap()->is_in(p), "pointer outside heap");

  // Test p and if it hasn't been claimed then push on the bdaref_stack
  if (p != NULL) {
    oop o = oopDesc::load_decode_heap_oop_not_null(p);
    if (o->is_forwarded()) {
      o = o->forwardee();
      // Clean the card
      if (PSScavenge::is_obj_in_young(o)) {
        PSScavenge::card_table()->inline_write_ref_field_gc(p, o);
      }
      oopDesc::encode_store_heap_oop_not_null(p, o);
    } else {
      push_bdaref_stack(p, ct);
    }
  }
}

template<bool promote_immediately> oop
PSPromotionManager::copy_bdaref_to_survivor_space(oop o, void * r, RefQueue::RefType rt)
{  
  // TODO: Promote immediately does not work for now, thus the oop o is promoted to
  // old without further tests

  oop new_obj = NULL;
  container_t container = NULL;
  MutableBDASpace * old_space = (MutableBDASpace *) old_gen()->object_space();

  markOop test_mark = o->mark();

  // The same as "o->is_forwarded()". Jumps right away since another thread won the race.
  if (!test_mark->is_marked()) {
    size_t new_obj_size = o->size();

    // If it is RefType::container
    if (rt) {
      // Allocate a container on the correct bda-space (already pushes new_obj_size)
      container = old_space -> allocate_container(new_obj_size, (BDARegion*)r);

      // Usually, the MutableBDASpace prepares for this scenario.
      // It allocates the new container in the general object space. However,
      // if the "other" space is also full, then it generally means that a FullGC
      // must take place.
      if (container == NULL) {
        _old_gen_is_full = true;
        return bda_oop_promotion_failed(o, test_mark);
      }

      // // Set the filling container for this promotion manager
      // set_filling_segment (container);
      
      // Now get the start ptr which is the parent object
      new_obj = (oop)container->_start;

      // Copy obj
      Copy::aligned_disjoint_words((HeapWord*)o, (HeapWord*)new_obj, new_obj_size);

      // Try to cas in the header. If it succeeds push the contents and pass the container_t
      // structure. If it fails just leave the space empty.
      if (o->cas_forward_to(new_obj, test_mark)) {
        assert (new_obj == o->forwardee(), "Sanity");

        if (new_obj_size > _min_array_size_for_chunking &&
            new_obj->is_objArray() &&
            PSChunkLargeArrays) {
          // chunk the array and push on the stack right away to continue processing
          oop* const masked_oop = mask_chunked_array_oop(o);
          push_bdaref_stack(masked_oop, container);
        } else {
          new_obj->push_bdaref_contents(this, container);
        }
      } else {
        // lost the cas header race
        guarantee(o->is_forwarded(), "Object must be forwarded if the cas failed.");
        // Deallocate the object. In this case, the container is left empty.
        container->_top = container->_start;
        // Dont't update this before the unallocation!
        new_obj = o->forwardee();
      }
    } else {
      // If it is RefType::element it must abide to the container_t info
      container = (container_t) r;

      // Tries to allocate. It fails if the lab has no space left or if the lab
      // is not targeted for this container/segment
      new_obj = (oop) _bda_old_lab.allocate (new_obj_size, container);

      if (new_obj == NULL) {
        if (new_obj_size > (BDAOldPLABSize / 2)) {
          // Allocate directly
          new_obj = (oop) old_space -> allocate_element (new_obj_size, container);
        } else {
          // Allocate new lab, flush and fill
          HeapWord * lab_base = old_space -> allocate_plab (container);
          if (lab_base != NULL) {
            _bda_old_lab.flush();
            _bda_old_lab.initialize (MemRegion(lab_base, BDAOldPLABSize), container);
            new_obj = (oop) _bda_old_lab.allocate (new_obj_size, container);
          }
        }
      }
      // // Here the container is changed accordingly and the new object pointer is returned
      // new_obj = (oop) old_space -> allocate_element(new_obj_size, container);

      if (new_obj == NULL) {
        _old_gen_is_full = true;
        return bda_oop_promotion_failed(o, test_mark);
      }

      // // Set the filling container for this promotion manager
      // set_filling_segment (container);

      // Copy obj
      Copy::aligned_disjoint_words((HeapWord*)o, (HeapWord*)new_obj, new_obj_size);

      // Try to cas in the header.
      if (o->cas_forward_to(new_obj, test_mark)) {
        assert (new_obj == o->forwardee(), "Sanity");

        if (new_obj_size > _min_array_size_for_chunking &&
            new_obj->is_objArray() &&
            PSChunkLargeArrays) {
          // chunk the array and push on the stack right away to continue processing
          oop* const masked_oop = mask_chunked_array_oop(o);
          push_bdaref_stack(masked_oop, container);
        } else {
          new_obj->push_bdaref_contents(this, container);
        }
      } else {
        // lost the cas header race
        guarantee(o->is_forwarded(), "Object must be forwarded if the cas failed.");
        // Unallocate the object
        if (!_bda_old_lab.unallocate_object ((HeapWord *) new_obj, new_obj_size)) {
          // If it could not unallocate, fill with a filler to leave this part unusable.
          CollectedHeap::fill_with_object((HeapWord*) new_obj, new_obj_size);
        }
        
        // Don't update this before the unallocation
        new_obj = o->forwardee();
      }
    }
  } else {
    assert (o->is_forwarded(), "Sanity");
    new_obj = o->forwardee();
  }

  return new_obj;
}

template <class T>
inline void
PSPromotionManager::process_popped_bdaref_depth(BDARefTask t)
{
  StarTask p((T*)t);
  container_t ct = t.container();
  if (is_oop_masked(p)) {
    assert(PSChunkLargeArrays, "invariant");
    oop const old = unmask_chunked_array_oop(p);
    process_bda_array_chunk(old, ct);
  } else {
      BDAScavenge::copy_and_push_safe_barrier<T, /*promote_immediately=*/true>(
        this, (T*)t, (void*)ct, RefQueue::element);
  }
}

template <class T>
inline void
PSPromotionManager::process_dequeued_bdaroot(Ref * r)
{
  // Ref only keeps oop and does not encode into an narrowOop
  if (PSScavenge::should_scavenge((T*)r->ref_addr())) {
    BDAScavenge::copy_and_push_safe_barrier<T, true>(this, (T*)r->ref_addr(), (void*)r->region(),
                                                     RefQueue::container);
  }
}

//
// This version is practically equal to process_array_chunk
// but it makes a different call on the claim_or_forward_depth call of
// the process_array_chunk_work<>
//
inline void
PSPromotionManager::process_bda_array_chunk(oop old, container_t c)
{
  assert(PSChunkLargeArrays, "invariant");
  assert(old->is_objArray(), "invariant");
  assert(old->is_forwarded(), "invariant");

  oop const obj = old->forwardee();

  int start;
  int const end = arrayOop(old)->length();
  if (end > (int) _min_array_size_for_chunking) {
    start = end - _array_chunk_size;
    assert (start > 0, "invariant");
    arrayOop(old)->set_length(start);
    push_bdaref_stack(mask_chunked_array_oop(old), c);
  } else {
    // the final chunk of the array
    start = 0;
    int const actual_length = arrayOop(obj)->length();
    arrayOop(old)->set_length(actual_length);
  }

  if (UseCompressedOops) {
    process_bda_array_chunk_work<narrowOop>(obj, start, end, c);
  } else {
    process_bda_array_chunk_work<oop>(obj, start, end, c);
  }
}

template <class T> void
PSPromotionManager::process_bda_array_chunk_work(oop obj,
                                                 int start,
                                                 int end,
                                                 container_t c)
{
  assert (start <= end, "invariant");
  T* const base      = (T*)objArrayOop(obj)->base();
  T* p               = base + start;
  T* const chunk_end = base + end;
  while (p < chunk_end) {
    if (PSScavenge::should_scavenge(p)) {
      claim_or_forward_bdaref(p, c);
    }
    p++;
  }
}
    
    
#endif

inline void PSPromotionManager::process_popped_location_depth(StarTask p) {
  if (is_oop_masked(p)) {
    assert(PSChunkLargeArrays, "invariant");
    oop const old = unmask_chunked_array_oop(p);
    process_array_chunk(old);
  } else {
    if (p.is_narrow()) {
      assert(UseCompressedOops, "Error");
      PSScavenge::copy_and_push_safe_barrier<narrowOop, /*promote_immediately=*/false>(this, p);
    } else {
      PSScavenge::copy_and_push_safe_barrier<oop, /*promote_immediately=*/false>(this, p);
    }
  }
}

#if TASKQUEUE_STATS
void PSPromotionManager::record_steal(StarTask& p) {
  if (is_oop_masked(p)) {
    ++_masked_steals;
  }
}
#endif // TASKQUEUE_STATS

#endif // SHARE_VM_GC_IMPLEMENTATION_PARALLELSCAVENGE_PSPROMOTIONMANAGER_INLINE_HPP

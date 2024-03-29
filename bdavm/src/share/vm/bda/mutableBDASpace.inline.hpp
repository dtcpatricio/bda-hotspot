#ifndef SHARE_VM_BDA_MUTABLEBDASPACE_INLINE_HPP
#define SHARE_VM_BDA_MUTABLEBDASPACE_INLINE_HPP

# include "bda/mutableBDASpace.hpp"
# include "oops/klassRegionMap.hpp"



// inline container_t
// MutableBDASpace::CGRPSpace::push_container(size_t size)
// {
//   container_t container;

//   // Objects bigger than this size are, generally, large arrays
//   // Get a container from the large_pool or allocate a special one
//   // big enough for the size.
//   if (size > MutableBDASpace::CGRPSpace::segment_sz) {

//     if (_large_pool->peek() == NULL) {
//       if (!reuse_from_pool(container, size)) {
//         container = allocate_large_container(size);
//       }
//     } else {
//       int tries = 0;
//       while ( (container = _large_pool->dequeue()) != NULL ) {
//         unmask_container(container);
//         if (pointer_delta(container->_end, container->_start) >= size) {
//           container->_top += size;
//           break;
//         } else if (tries < 3) {
//           mask_container(container);
//           _large_pool->enqueue(container);
//           tries++;
//         } else {
//           if (!reuse_from_pool(container, size)) {
//             container = allocate_large_container(size);
//           }
//           break;
//         }
//       }
//     }

//     // Else, allocate a normal sized container based on the average container size
//     // using the arguments provided at startup (allocate_container() in mutableBDASpace.cpp).
//   } else {
  
//     if ( (container = _pool->dequeue()) != NULL ) {
//       unmask_container(container);
//       container->_top += size;
//     } else {
//       // This call already sets the top
//       container = allocate_container(size);
//     }
//   }

//   // Add to the queue
//   if (container != NULL) {
//     assert (container->_next_segment == NULL, "should have been reset");
//     assert (container->_prev_segment == NULL, "should have been reset");
// #ifdef ASSERT
//     _segments_since_last_gc++;
// #endif
//     // MT safe, but only for subsequent enqueues/dequeues. If mixed, then the queue may break!
//     // See gen_queue.hpp for more details.
//     _containers->enqueue(container);
//   }

//   return container;
// }



// inline HeapWord *
// MutableBDASpace::CGRPSpace::allocate_new_segment(size_t size, container_t& c)
// {
//   container_t container;
//   container_t next;
//   container_t last;

//   // Objects bigger than this size are, generally, large arrays
//   // Get a container from the large_pool or allocate a special one
//   // big enough for the size.
//   if ( size > MutableBDASpace::CGRPSpace::segment_sz ) {

//     if (_large_pool->peek() == NULL) {
//       if (!reuse_from_pool(container, size)) {
//         container = allocate_large_container(size);
//       }
//     } else {
//       int tries = 0;
//       while ( (container = _large_pool->dequeue()) != NULL ) {
//         unmask_container(container);
//         if (pointer_delta(container->_end, container->_start) >= size) {
//           container->_top += size;
//           break;
//         } else if (tries < 3) {
//           mask_container(container);
//           _large_pool->enqueue(container);
//           tries++;
//         } else {
//           if (!reuse_from_pool(container, size)) {
//             container = allocate_large_container(size);
//           }
//           break;
//         }
//       }
//     }

//     // Else, allocate a normal sized container based on the average container size
//     // using the arguments provided at startup (allocate_container() in mutableBDASpace.cpp).
//   } else {
    
//     if ( (container = _pool->dequeue() ) != NULL ) {
//       unmask_container(container);
//       container->_top += size;
//     } else {
//       container = allocate_container(size);
//     }
    
//   }

//   // The non-bda-space cannot have linked segments because these are deallocated after
//   // FullGC, causing a load of invalid space from a parent allocated in a bda-space (which are
//   // not deallocated).
//   if (container != NULL) {
// #ifdef ASSERT
//     _segments_since_last_gc++;
// #endif
//     // MT safe, but only for subsequent enqueues/dequeues.
//     _containers->enqueue(container);

//     // only update links in bda-spaces
//     if (container_type() != KlassRegionMap::region_start_ptr()) {
//       last = c; next = last->_next_segment;
//       do {
//         if (next == NULL && Atomic::cmpxchg_ptr(container,
//                                                 &(last->_next_segment),
//                                                 next) == next) {
//           break;
//         }
//         last = last->_next_segment;
//         next = last->_next_segment;
//       } while (true);

//       container->_prev_segment = last;
//     }

//     // For the return val
//     c = container;
//     return container->_start;
//   } else {    
//     return NULL;
//   }
// }

inline void
MutableBDASpace::CGRPSpace::free_container(container_t container)
{
  // Assert that the container references no other.
  assert (container->_prev_segment == NULL && container->_next_segment == NULL,
          "segment associated with parent container");
  memset(container, 0, sizeof(struct container));
  _pool->enqueue(container);
}

inline size_t
MutableBDASpace::CGRPSpace::calculate_reserved_sz()
{
  size_t reserved_sz_bytes = 0;
  size_t reserved_sz = 0;
  const size_t f = sizeof(HeapWord*); // default field size
  const size_t h = 2 * sizeof(HeapWord*); // size of header;
  HeapWord * ptr = NULL;

  // Reserve space in this bda-space's MutableSpace.
  // First calculate the necessary space, in heapwords, according to the
  // default collection size, the amount of delegation -- delegation-level or dl --,
  // and the default number of fields (dnf) for each element.

  // The algorithm computes the following expression, where 'f' is the default size
  // of fields (assumed to be 1 hw each), 'h' the header size and 'k' the
  // DELEGATION_LEVEL, i.e., how much the elements of a collection delegate until
  // the actual data:
  // [ SUM_n=1 to k (f * dnf^n + h * dnf^n) ] + h + f * dnf^k

  const size_t term1 = dnf * f;
  const size_t term2 = dnf * delegation_level * (f + h);
  const size_t term3 = (f + h) + node_fields * ( (f + h) + term1 + term2);
  
  reserved_sz_bytes = term3 * default_collection_size * BDAContainerFraction;

  // First, align the reserved_sz with the MinRegionSize. This is important
  // so that the ParallelOld doesn't split containers in half.
  reserved_sz = (size_t) align_size_up((intptr_t)reserved_sz_bytes >> LogHeapWordSize,
                                       MutableBDASpace::MinRegionSize);

  return reserved_sz;
}

inline size_t
MutableBDASpace::CGRPSpace::calculate_large_reserved_sz(size_t size)
{
  size_t reserved_sz = 0;
  reserved_sz = (size_t) align_size_up((intptr_t)size, segment_sz);
  return reserved_sz;
}

inline container_t
MutableBDASpace::CGRPSpace::install_container_in_space(size_t reserved_sz, size_t size)
{
  HeapWord * ptr;
  
  // Now the amount of space is reserved with a CAS and the resulting ptr
  // is returned as the start of the collection struct
  
  ptr = space()->cas_allocate(reserved_sz);

  // If there's no space left to allocate a new container, then
  // return NULL so that MutableBDASpace can handle the allocation
  // of a new container to "other" space.
  if (ptr == NULL) {
    return (container_t)ptr;
  }
  
  // We allocate it with the general AllocateHeap since struct is not, by default,
  // subclass of one of the base VM classes (CHeap, ResourceObj, ... and friends).
  // It must be initialized prior to allocation because there's no default constructor.
  container_t container = (container_t)AllocateHeap(sizeof(struct container), mtGC);
  container->_start = ptr; container->_top = ptr + size;
  container->_end = ptr + reserved_sz - MutableBDASpace::_filler_header_size;
  container->_next_segment = NULL; container->_next = NULL; container->_prev_segment = NULL;
  container->_previous = NULL; container->_saved_top = NULL;
  container->_space_id = (char)_type->value();
  container->_hard_end = ptr + reserved_sz;

  return container;
}
  
inline bool
MutableBDASpace::CGRPSpace::clear_delete_containers()
{
  if (container_count() > 0) {
    container_t c = _containers->peek();
    int const count = _containers->n_elements();
    int           i = 0;
#ifdef BDA_PARANOID
    _manager->verify_segments_in_othergen();
#endif
    while (i++ < count) {
      if (c->_prev_segment != NULL) {
#ifdef BDA_PARANOID
        // the value 0 is hard-coded since it means it refers to the other-space
        int spaceid = c->_prev_segment->_space_id;
        if (spaceid != 0) {
          gclog_or_tty->print_cr("(PARANOID) Segment " INTPTR_FORMAT " is extension of "
                                 INTPTR_FORMAT " from space " INT32_FORMAT,
                                 (intptr_t)c, (intptr_t)c->_prev_segment, spaceid);
        }
#endif // BDA_PARANOID
        c->_prev_segment->_next_segment = c->_next_segment;
      }
      if (c->_next_segment != NULL) {
#ifdef BDA_PARANOID
        // the value 0 is hard-coded since it means it refers to the other-space
        int spaceid = c->_next_segment->_space_id;
        if (spaceid != 0) {
          gclog_or_tty->print_cr("(PARANOID) Segment " INTPTR_FORMAT " extends of " INTPTR_FORMAT
                                 " from space " INT32_FORMAT,
                                 (intptr_t)c, (intptr_t)c->_prev_segment, spaceid);
        }
#endif // BDA_PARANOID
        c->_next_segment->_prev_segment = c->_prev_segment;
      }
      // do this first, before enqueuing (it sets _next and _previous).
      container_t n = c->_next;
      memset(c, 0, sizeof(struct container));
      _pool->enqueue_no_mt(c);
      c = n;
    }
    GenQueue<container_t, mtGC>::destroy(_containers);
    // TODO: This mess should change, such as the PARANOID stuff, the place it is called and
    // the strings it prints.
#ifdef BDA_PARANOID
    _manager->verify_segments_in_othergen();
#endif
  }
  assert (_containers->n_elements() == 0, "list shouldn't have containers.");
}

inline void
MutableBDASpace::CGRPSpace::mask_container(container_t& c)
{
  assert (((uintptr_t)c->_end & CONTAINER_IN_POOL_MASK) == 0, "Information loss!");
  c->_end = (HeapWord*)((uintptr_t)c->_end | CONTAINER_IN_POOL_MASK);
}

inline void
MutableBDASpace::CGRPSpace::unmask_container(container_t& c)
{
  assert (((uintptr_t)c->_end & CONTAINER_IN_POOL_MASK) != 0, "Malformed masking!");
  c->_end = (HeapWord*)((uintptr_t)c->_end & ~CONTAINER_IN_POOL_MASK);
}
 
inline bool
MutableBDASpace::CGRPSpace::not_in_pool(container_t c) const
{
  return (((uintptr_t)c->_end & CONTAINER_IN_POOL_MASK) == 0);
}

inline void
MutableBDASpace::CGRPSpace::add_to_pool(container_t c)
{
  if (not_in_pool(c)) {
    _containers->remove_element(c);
    // Reset some fields (previous doesn't need to be reset because it will be set)
    assert (c->_top == c->_start, "container is not empty");
    memset(c, 0, sizeof(struct container));
    mask_container(c);
    c->_space_id = (char)(exact_log2((intptr_t) _type->value()));
    _pool->enqueue(c);
  }
}

inline void
MutableBDASpace::CGRPSpace::remove_from_pool (container_t c)
{
  _pool->remove_element_mt (c);
}

inline int
MutableBDASpace::CGRPSpace::power_function(int base, int exp)
{
  int acc = 0;
  exp--;
  acc += base << exp;
  for (int i = exp - 1; i > 0; i--) acc += exp * (base << i);
  acc += base;
}

inline container_t
MutableBDASpace::CGRPSpace::get_next_n_segment(container_t c, int n) const
{
  assert (c != NULL, "container/segment cannot be null");
  int i = 0; container_t ret = c;
  while (i++ < n && ret != NULL) {
    ret = ret->_next;
  }
  return ret;
}

inline container_t
MutableBDASpace::CGRPSpace::get_previous_n_segment(container_t c, int n) const
{
  assert (c != NULL, "container/segment cannot be null");
  int i = 0; container_t ret = c;
  while (i++ < n && ret != NULL) {
    ret = ret->_previous;
  }
  return ret;
}

inline container_t
MutableBDASpace::CGRPSpace::get_container_with_addr(HeapWord* addr) const
{
  container_t c;
  for (GenQueueIterator<container_t, mtGC> iterator = _containers->iterator();
       *iterator != NULL;
       ++iterator) {
    c = *iterator;
    if (addr == c->_start)
      return c;
  }
  return NULL;
}

inline void
MutableBDASpace::CGRPSpace::save_top_ptrs()
{
  if (container_count() > 0) {
    _last_segment = last_container();
    for(GenQueueIterator<container_t, mtGC> iterator = _containers->iterator();
        *iterator != NULL;
        ++iterator) {
      container_t c = *iterator;
      c->_saved_top = c->_top;
#ifdef ASSERT
      c->_scanned_flag = -1;
#endif 
    }
  } else {
    _last_segment = NULL;
  }
}

inline size_t
MutableBDASpace::CGRPSpace::used_in_words() const
{
  size_t s = 0;
  // If this is the other's space, we account from start to top.
  if (container_type() != KlassRegionMap::region_start_ptr() && container_count() > 0) {
    for (GenQueueIterator<container_t, mtGC> it = _containers->iterator();
         *it != NULL;
         ++it)
    {
      container_t c = *it;
      s += pointer_delta(c->_top, c->_start);
    }
  } else {
    return space()->used_in_words();
  }
  return s;
}

inline size_t
MutableBDASpace::CGRPSpace::free_in_words() const
{
  size_t s = 0;
  // If this is the other's space, we account from top to end
  if (container_type() != KlassRegionMap::region_start_ptr() && container_count() > 0) {
    for (GenQueueIterator<container_t, mtGC> it = _containers->iterator();
         *it != NULL;
         ++it)
    {
      container_t c = *it;
      s += pointer_delta(c->_end, c->_top);
    }
  } else {
    return space()->free_in_words();
  }
  return s;
}
inline size_t
MutableBDASpace::CGRPSpace::fast_free_in_words() const
{
  return space()->free_in_words();
}
inline size_t
MutableBDASpace::CGRPSpace::free_in_bytes() const
{
  return free_in_words() * HeapWordSize;
}
inline size_t
MutableBDASpace::CGRPSpace::fast_free_in_bytes() const
{
  return fast_free_in_words() * HeapWordSize;
}
inline size_t
MutableBDASpace::CGRPSpace::used_in_bytes() const
{
  return used_in_words() * HeapWordSize;
}
/////////////////////////////////////////
// MutableBDASpace inline Definitions////
/////////////////////////////////////////
inline int
MutableBDASpace::container_count()
{
  int acc = 0;
  for (int i = 1; i < _spaces->length(); ++i) {
    acc += _spaces->at(i)->container_count();
  }
  return acc;
}

inline void
MutableBDASpace::save_tops_for_scavenge()
{
  for (int i = 0; i < spaces()->length(); ++i) {
    spaces()->at(i)->save_top_ptrs();
  }
}

inline bool
MutableBDASpace::is_bdaspace_empty()
{
  bool ret = true;
  for (int i = 1; i < _spaces->length(); ++i) {
    if (_spaces->at(i)->space()->is_empty())
      ret &= true;
    else
      ret &= false;
  }
  return ret;
}

// This marks the newly allocated container in the segment bitmap for posterior usage
// in the YoungGC
inline bool
MutableBDASpace::mark_container(container_t c)
{
  return _segment_bitmap.mark_obj(c->_start, pointer_delta(c->_hard_end, c->_start));
}

// This unmarks the container in the segment bitmap
inline void
MutableBDASpace::unmark_container(container_t c)
{
  const idx_t beg_bit = _segment_bitmap.addr_to_bit (c->_start);
  const idx_t end_bit = _segment_bitmap.addr_to_bit (c->_hard_end - 1);
  _segment_bitmap.clear_range(beg_bit, end_bit);
}

inline void
MutableBDASpace::allocate_block(HeapWord * obj)
{
  _start_array->allocate_block(obj);
}

inline HeapWord *
MutableBDASpace::get_next_beg_seg(HeapWord * beg, HeapWord * end) const
{
  _segment_bitmap.find_obj_beg(beg, end);
}

inline HeapWord *
MutableBDASpace::get_next_end_seg(HeapWord * beg, HeapWord * end) const
{
  _segment_bitmap.find_obj_end(beg, end);
}
#endif

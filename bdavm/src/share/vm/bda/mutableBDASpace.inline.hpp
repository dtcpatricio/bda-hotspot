#ifndef SHARE_VM_BDA_MUTABLEBDASPACE_INLINE_HPP
#define SHARE_VM_BDA_MUTABLEBDASPACE_INLINE_HPP

# include "bda/mutableBDASpace.hpp"

inline container_t*
MutableBDASpace::CGRPSpace::push_container(size_t size)
{
  container_t * container;

  // Objects bigger than this size are, generally, large arrays
  // Get a container from the large_pool or allocate a special one
  // big enough for the size.
  if (size > MutableBDASpace::MinRegionSize) {

    int tries = 0;
    while ( (container = _large_pool->dequeue()) != NULL ) {
      unmask_container(container);
      if (pointer_delta(container->_end, container->_start) >= size) {
        container->_top += size;
        break;
      } else if (tries < 3) {
        _large_pool->enqueue(container);
        tries++;
      } else {
        container = allocate_large_container(size);
        break;
      }
    }

  // Else, allocate a normal sized container based on the average container size
  // using the arguments provided at startup (allocate_container() in mutableBDASpace.cpp).
  } else {
  
    if ( (container = _pool->dequeue()) != NULL ) {
      unmask_container(container);
      container->_top += size;
    } else {
      container = allocate_container(size);
    }
  }

  // Add to the pool
  if (container != NULL) {
    assert (container->_next_segment == NULL, "should have been reset");
    assert (container->_prev_segment == NULL, "should have been reset");
    // MT safe, but only for subsequent enqueues/dequeues. If mixed, then the queue may break!
    // See gen_queue.hpp for more details.
    _containers->enqueue(container);
  }

  return container;
}

inline HeapWord *
MutableBDASpace::CGRPSpace::allocate_new_segment(size_t size, container_t ** c)
{
  container_t * container;
  container_t * next_segment;
  container_t * prev_segment;

  // Objects bigger than this size are, generally, large arrays
  // Get a container from the large_pool or allocate a special one
  // big enough for the size.
  if ( size > MutableBDASpace::MinRegionSize ) {

    int tries = 0;
    while ( (container = _large_pool->dequeue()) != NULL ) {
      unmask_container(container);
      if (pointer_delta(container->_end, container->_start) >= size) {
        container->_top += size;
        break;
      } else if (tries < 3) {
        _large_pool->enqueue(container);
        tries++;
      } else {
        container = allocate_large_container(size);
        break;
      }
    }

  // Else, allocate a normal sized container based on the average container size
  // using the arguments provided at startup (allocate_container() in mutableBDASpace.cpp).
  } else {
    
    if ( (container = _pool->dequeue() ) != NULL ) {
      unmask_container(container);
      container->_top += size;
    } else {
      container = allocate_container(size);
    }
    
  }
  
  if (container != NULL) {
    next_segment = *c;
    do {
      prev_segment = next_segment;
      next_segment = prev_segment->_next_segment;
    } while (Atomic::cmpxchg_ptr(container,
                                 &(prev_segment->_next_segment),
                                 next_segment) != next_segment);
    container->_prev_segment = prev_segment;
    *c = container;
    // MT safe, but only for subsequent enqueues/dequeues.
    _containers->enqueue(container);
  } else {    
    return NULL;
  }
  
  return container->_start;
}

inline size_t
MutableBDASpace::CGRPSpace::calculate_reserved_sz()
{
  size_t reserved_sz = 0;
  size_t f = sizeof(HeapWord*); // default field size
  size_t h = 2 * sizeof(HeapWord*); // size of header;
  HeapWord * ptr = NULL;

  // Reserve space in this bda-space's MutableSpace.
  // First calculate the necessary space, in heapwords, according to the
  // default collection size, the amount of delegation -- delegation-level or dl --,
  // and the default number of fields (dnf) for each element.

  // The algorithm computes the following expression, where 'f' is the default number
  // of fields (assumed to be 1 hw each), 'h' the header size and 'k' the
  // DELEGATION_LEVEL, i.e., how much the elements of a collection delegate until
  // the actual data:
  // [ SUM_n=1 to k (f * dnf^n + h * dnf^n) ] + h + f * dnf^k

  for (int n = 1; n <= delegation_level; n++) {
    reserved_sz += power_function(dnf, n) * f + power_function(dnf, n) * h;
  }
  reserved_sz += h + power_function(dnf, delegation_level) * f;
  reserved_sz *= default_collection_size * BDAContainerFraction;

  // First, align the reserved_sz with the MinRegionSize. This is important
  // so that the ParallelOld doesn't split containers in half.
  reserved_sz = (size_t) align_size_up((intptr_t)reserved_sz, MutableBDASpace::MinRegionSize);

  return reserved_sz;
}

inline size_t
MutableBDASpace::CGRPSpace::calculate_large_reserved_sz(size_t size)
{
  size_t reserved_sz = 0;
  reserved_sz = (size_t) align_size_up((intptr_t)size, MutableBDASpace::MinRegionSize);
  return reserved_sz;
}

inline container_t *
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
    return (container_t*)ptr;
  }
  
  // We allocate it with the general AllocateHeap since struct is not, by default,
  // subclass of one of the base VM classes (CHeap, ResourceObj, ... and friends).
  // It must be initialized prior to allocation because there's no default constructor.
  container_t* container = (container_t*)AllocateHeap(sizeof(container_t), mtGC);
  container->_start = ptr; container->_top = ptr + size; container->_end = ptr + reserved_sz;
  container->_next_segment = NULL; container->_next = NULL; container->_prev_segment = NULL;
  container->_previous = NULL;
#ifdef ASSERT
  container->_space_id = (char)_type->value();
#endif

  return container;
}

inline void
MutableBDASpace::CGRPSpace::mask_container(container_t *& c)
{
  assert (((uintptr_t)c->_end & CONTAINER_IN_POOL_MASK) == 0, "Information loss!");
  c->_end = (HeapWord*)((uintptr_t)c->_end | CONTAINER_IN_POOL_MASK);
}

inline void
MutableBDASpace::CGRPSpace::unmask_container(container_t *& c)
{
  assert (((uintptr_t)c->_end & CONTAINER_IN_POOL_MASK) != 0, "Malformed masking!");
  c->_end = (HeapWord*)((uintptr_t)c->_end & ~CONTAINER_IN_POOL_MASK);
}
 
inline bool
MutableBDASpace::CGRPSpace::not_in_pool(container_t * c)
{
  return (((uintptr_t)c->_end & CONTAINER_IN_POOL_MASK) == 0);
}

inline void
MutableBDASpace::CGRPSpace::add_to_pool(container_t * c)
{
  if (not_in_pool(c)) {
    _containers->remove_element(c);
    mask_container(c);
    // Reset some fields (previous doesn't need to be reset because it will be set)
    c->_next_segment = NULL; c->_next = NULL; c->_prev_segment = NULL;
    if (pointer_delta(c->_end, c->_start) > segment_sz)
      _large_pool->enqueue(c);
    else
      _pool->enqueue(c);
  }
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

// The point of this code is to keep traversing the queue while checking if gc_current
// is a container, i.e., it must have prev_segment equal to NULL.
// This is such, because GC threads will follow all segments of a container on their own, without
// interference.
// Another way is changing the way segments are linked by using the next_segment ptr only, and
// keeping the next ptr only for parent containers. This may have drawbacks in the scanning of all
// segments (for example, during OldGC post compact phase).
inline container_t *
MutableBDASpace::CGRPSpace::cas_get_next_container()
{
  container_t * c = NULL;
  while (_gc_current != NULL) {
    c = _gc_current;
    if (c == NULL)
      break;
    else if ((Atomic::cmpxchg_ptr(c->_next, &_gc_current, c) == c) &&
             c->_prev_segment == NULL)
      break;
  }
  return c;
}

inline container_t *
MutableBDASpace::CGRPSpace::get_container_with_addr(HeapWord* addr)
{
  container_t * c;
  for (GenQueueIterator<container_t *, mtGC> iterator = _containers->iterator();
       *iterator != NULL;
       ++iterator) {
    c = *iterator;
    if (addr == c->_start)
      return c;
  }
  return NULL;
}

inline void
MutableBDASpace::CGRPSpace::save_top_ptrs(container_helper_t * helper, int * i)
{
  int from = *i;

  for(GenQueueIterator<container_t*, mtGC> iterator = _containers->iterator();
      *iterator != NULL;
      ++iterator) {
    container_t * c = *iterator;
    helper[from]._container = c;
    helper[from++]._top = c->_top;
  }

  *i = from;
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

#endif

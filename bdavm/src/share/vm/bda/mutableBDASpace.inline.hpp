#ifndef SHARE_VM_BDA_MUTABLEBDASPACE_INLINE_HPP
#define SHARE_VM_BDA_MUTABLEBDASPACE_INLINE_HPP

# include "bda/mutableBDASpace.hpp"

// Definition of minimum and aligned size of a container. Subject to change
// if it is too low. This values are conform to the ParallelOld (psParallelCompact.cpp).
const size_t MutableBDASpace::Log2BlockSize   = 7; // 128 words
const size_t MutableBDASpace::BlockSize       = (size_t)1 << Log2BlockSize;
const size_t MutableBDASpace::BlockSizeBytes  = BlockSize << LogHeapWordSize;

// Definition of sizes for the calculation of a collection size
int MutableBDASpace::CGRPSpace::dnf = 0;
int MutableBDASpace::CGRPSpace::delegation_level = 0;
int MutableBDASpace::CGRPSpace::default_collection_size = 0;
int MutableBDASpace::CGRPSpace::initial_array_sz = 0;

inline container_t*
MutableBDASpace::CGRPSpace::push_container(size_t size)
{
  size_t reserved_sz = 0;
  size_t f = sizeof(HeapWord*); // default field size
  size_t h = 2 * sizeof(HeapWord*); // size of header;
  HeapWord* ptr = NULL;

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
  reserved_sz *= default_collection_size;

  // First, align the reserved_sz with the MinRegionSize. This is important
  // so that the ParallelOld doesn't split containers in half.
  reserved_sz = (size_t) align_size_up((intptr_t)reserved_sz, MutableBDASpace::BlockSizeBytes);
  
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
  // It must be initialized prior to allocation since there's no default constructor.
  container_t* container = (container_t*)AllocateHeap(sizeof(container_t), mtGC);
  container->_start = ptr; container->_top = ptr + size; container->_end = ptr + reserved_sz;
#ifdef ASSERT
  container->_size = 0;
  container->_space_id = (char)_type->value();
  container->_reserved = reserved_sz;
#endif

  // FIXME: NOT MT safe! Use SynchronizedGrowableArray
  _containers->append(container);

  return container;
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

#endif

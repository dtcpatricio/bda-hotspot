#ifndef SHARE_VM_BDA_MUTABLEBDASPACE_INLINE_HPP
#define SHARE_VM_BDA_MUTABLEBDASPACE_INLINE_HPP

# include "bda/mutableBDASpace.hpp"

int MutableBDASpace::CGRPSpace::dnf = 0;
int MutableBDASpace::CGRPSpace::delegation_level = 0;
int MutableBDASpace::CGRPSpace::default_collection_size = 0;

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

  // Now the amount of space is reserved with a CAS and the resulting ptr
  // is returned as the start of the collection struct

  ptr = space()->cas_allocate(reserved_sz);

  container_t* container = ::new container_t;
  container->_top = ptr; container->_start = ptr;
#ifdef ASSERT
  container->_size = 0;
  container->_space_id = (char)_type->value();
  container->_reserved = reserved_sz;
#endif

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

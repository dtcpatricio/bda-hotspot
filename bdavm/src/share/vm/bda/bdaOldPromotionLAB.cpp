#include "bda/bdaOldPromotionLAB.hpp"
#include "gc_implementation/parallelScavenge/parallelScavengeHeap.hpp"


// This is the shared initialization code. It sets up the basic pointers,
// and allows enough extra space for a filler object. We call a virtual
// method, "lab_is_valid()" to handle the different asserts the old/young
// labs require.
void BDAOldPromotionLAB::initialize(MemRegion lab, container_t container)
{
  _container = container;
  
  assert(lab_is_valid(lab), "Sanity");

  HeapWord* bottom = lab.start();
  HeapWord* end    = lab.end();

  set_bottom(bottom);
  set_end(end);
  set_top(bottom);

  // Initialize after VM starts up because header_size depends on compressed
  // oops.
  filler_header_size = align_object_size(typeArrayOopDesc::header_size(T_INT));

  // We can be initialized to a zero size!
  if (free() > 0) {
    if (ZapUnusedHeapArea) {
      debug_only(Copy::fill_to_words(top(), free()/HeapWordSize, badHeapWord));
    }

    // NOTE! We need to allow space for a filler object.
    assert(lab.word_size() >= filler_header_size, "lab is too small");
    end = end - filler_header_size;
    set_end(end);

    _state = needs_flush;
  } else {
    _state = zero_size;
  }

  assert(this->top() <= this->end(), "pointers out of order");
}

HeapWord*
BDAOldPromotionLAB::allocate(size_t size, container_t container)
{
  if (_container != container) {
    return NULL;
  } else {
    return PSOldPromotionLAB::allocate (size);
  }
}

#ifdef ASSERT
bool
BDAOldPromotionLAB::lab_is_valid (MemRegion lab)
{
  if (_container == NULL)
    return true;
  else {
    MemRegion used = MemRegion (_container->_start, _container->_top);
    return used.contains(lab);
  }
}
#endif // ASSERT

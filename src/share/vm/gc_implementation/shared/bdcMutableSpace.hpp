#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_BDCMUTABLESPACE_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_BDCMUTABLESPACE_HPP

#include "gc_implementation/shared/bdaGlobals.hpp"
#include "gc_implementation/shared/mutableSpace.hpp"

// Implementation of an allocation space for big-data collection placement


// Forward declarations
class Thread;
class CollectedHeap;
class SpaceDecorator;

// The BDCMutableSpace class is a general object that encapsulates multiple
// CGRPSpaces. It is implemented in a similar fashion as mutableNUMASpace.

class BDCMutableSpace : public MutableSpace {
  friend class VMStructs;

  // This class defines the addressable space of the BDCMutableSpace
  // (and that of a PLAB)for a particular collection type, or none at all.
  class CGRPSpace : public CHeapObj<mtGC> {

    enum CollectionShiftConstants {
      collection_shift               = 1,
      collection_offset              = 0x1
    };

    BDACollectionType _coll_type;
    MutableSpace* _space;

  public:
    CGRPSpace(size_t alignment, BDACollectionType coll_type) : _coll_type(coll_type) {
      _space = new MutableSpace(alignment);
    }
    ~CGRPSpace() {
      // We have nothing to delete, though we used to have _space.
      // However, now we cannot make it a pointer to the MutableSpace
      // since that region must live.
    }

    static bool equals(void* group_type, CGRPSpace* s) {
      return *(BDACollectionType*)group_type == s->coll_type();
    }

    BDACollectionType coll_type() const { return _coll_type; }
    MutableSpace* space() const { return _space; }
  };

private:
  GrowableArray<CGRPSpace*>* _collections;
  Monitor *_lock;
  size_t _page_size;

protected:
  void select_limits(MemRegion mr, HeapWord **start, HeapWord **tail);

 public:

  BDCMutableSpace(size_t alignment);
  virtual ~BDCMutableSpace();

  void set_page_size(size_t page_size) { _page_size = page_size; }
  size_t page_size() const { return _page_size; }
  GrowableArray<CGRPSpace*>* collections() const { return _collections; }

  virtual void      initialize(MemRegion mr,
                               bool clear_space,
                               bool mangle_space,
                               bool setup_pages = SetupPages);

  // Boolean queries - the others are already implemented on mutableSpace.hpp
  bool contains(const void* p) const {
    assert(collections() != NULL, "Collections array no initialized");
    for(int i = 0; i < collections()->length(); ++i) {
      if(collections()->at(i)->space()->contains((HeapWord*)p))
        return true;
    }
    return false;
  }

  // This method updated the top of the whole region by checking the max
  // tops of the whole CGRPspaces
  bool update_top();

  HeapWord *top_specific(BDACollectionType type) {
    int i = _collections->find(&type, CGRPSpace::equals);
    return _collections->at(i)->space()->top();
  }

  // Methods for mangling
  virtual void set_top_for_allocations(HeapWord *v) PRODUCT_RETURN;
  virtual void set_top_for_allocations() PRODUCT_RETURN;

  // Size computations in heapwords.
  virtual size_t used_in_words() const;
  virtual size_t free_in_words() const;

  // Size computations for tlabs
  using MutableSpace::capacity_in_words;
  virtual size_t capacity_in_words(Thread *thr) const;
  virtual size_t tlab_capacity(Thread *thr) const;
  virtual size_t tlab_used(Thread *thr) const;
  virtual size_t unsafe_max_tlab_alloc(Thread *thr) const;

  virtual HeapWord* allocate(size_t size);
  virtual HeapWord* cas_allocate(size_t size);

  virtual void      clear(bool mangle_space);
  virtual void      set_top(HeapWord* value);
  virtual void      set_end(HeapWord* value) { _end = value; }
};

#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_BDCMUTABLESPACE_HPP

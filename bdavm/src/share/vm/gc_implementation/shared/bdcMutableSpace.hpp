#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_BDCMUTABLESPACE_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_BDCMUTABLESPACE_HPP

#include "gc_implementation/shared/bdaGlobals.hpp"
#include "gc_implementation/shared/mutableSpace.hpp"

// Implementation of an allocation space for big-data collection placement


// Forward declarations
class Thread;
class CollectedHeap;
class SpaceDecorator;
class BDCMutableSpace;

// The BDACardTableHelper
class BDACardTableHelper : public CHeapObj<mtGC> {

private:
  int _length;
  HeapWord** _tops;
  MutableSpace** _spaces;

public:

  BDACardTableHelper(BDCMutableSpace* sp);
  ~BDACardTableHelper();

  int        length() const { return _length; }
  HeapWord** tops() const { return _tops; }
  MutableSpace** spaces() const { return _spaces; }
};

// The BDCMutableSpace class is a general object that encapsulates multiple
// CGRPSpaces. It is implemented in a similar fashion as mutableNUMASpace.

class BDCMutableSpace : public MutableSpace
{
  friend class VMStructs;

public:
  // Constant sizes in HeapWords, unless stated otherwise
  static const size_t Log2MinRegionSize;
  static const size_t MinRegionSize;
  static const size_t MinRegionSizeBytes;

  static const size_t MinRegionSizeOffsetMask;
  static const size_t MinRegionAddrOffsetMask;
  static const size_t MinRegionAddrMask;

private:
 
  // This class defines the addressable space of the BDCMutableSpace
  // for a particular collection type, or none at all.
  class CGRPSpace : public CHeapObj<mtGC> {
    
    MutableSpace* _space;
    BDARegion*    _coll_type;    

  public:
    CGRPSpace(size_t alignment, BDARegion* region) : _coll_type(region) {
      _space = new MutableSpace(alignment);
    }
    ~CGRPSpace() {
      delete _space;
    }

    static bool equals(void* group_type, CGRPSpace* s) {
      return *(BDARegion**)group_type == s->coll_type();
    }

    BDARegion*    coll_type() const { return _coll_type; }
    MutableSpace* space() const { return _space; }
    
  };

private:
  GrowableArray<CGRPSpace*>* _collections;
  size_t _page_size;

protected:
  void select_limits(MemRegion mr, HeapWord **start, HeapWord **tail);
  // To update the regions when resize takes place
  void update_layout(MemRegion mr);
  HeapWord* expand_overflown_neighbour(int i, size_t sz);

  // Expanding funtions
  void expand_region_to_neighbour(int i, size_t sz);
  bool try_fitting_on_neighbour(int moved_id);
  void initialize_regions(size_t space_size,
                          HeapWord* start,
                          HeapWord* end);
  // deprecated
  void initialize_regions_evenly(int from_id, int to_id,
                                 HeapWord* start_limit,
                                 HeapWord* end_limit,
                                 size_t space_size);
  void merge_regions(int growee, int eater);
  void move_space_resize(MutableSpace* spc, HeapWord* to_ptr, size_t sz);
  void shrink_and_adapt(int grp);

  // Increases of regions
  void increase_space_noclear(MutableSpace* spc, size_t sz);
  void increase_space_set_top(MutableSpace* spc, size_t sz, HeapWord* new_top);
  void grow_through_neighbour(MutableSpace* growee, MutableSpace* eaten, size_t sz);

  // Shrinks of regions
  void shrink_space_clear(MutableSpace* spc, size_t sz);
  void shrink_space_noclear(MutableSpace* spc, size_t sz);
  void shrink_space_end_noclear(MutableSpace *spc, size_t sz);

public:

  BDCMutableSpace(size_t alignment);
  virtual ~BDCMutableSpace();

  void set_page_size(size_t page_size) { _page_size = page_size; }
  size_t page_size() const { return _page_size; }
  GrowableArray<CGRPSpace*>* collections() const { return _collections; }
  MutableSpace* region_for(BDARegion* region) const {
    int i = _collections->find(&region, CGRPSpace::equals);
    return _collections->at(i)->space();
  }

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

  // This method adjusts the regions to give precedence to the one with the most
  // occupancy rate, which indicates it to be the needy one
  bool adjust_layout(bool force);
  size_t compute_avg_freespace();

  virtual HeapWord *top_specific(BDARegion* type) {
    int i = _collections->find(&type, CGRPSpace::equals);
    return _collections->at(i)->space()->top();
  }

  virtual MemRegion used_region(BDARegion* type) {
    int i = _collections->find(&type, CGRPSpace::equals);
    return _collections->at(i)->space()->used_region();
  }

  virtual int num_bda_regions() { return _collections->length() - 1; }

  // Methods for mangling
  virtual void set_top_for_allocations(HeapWord *v);
  virtual void set_top_for_allocations();

  // Size computations in heapwords.
  virtual size_t used_in_words() const;
  virtual size_t free_in_words() const;
  virtual size_t free_in_words(int grp) const;
  virtual size_t free_in_bytes(int grp) const;

  // Size computations for tlabs
  using MutableSpace::capacity_in_words;
  virtual size_t capacity_in_words(Thread *thr) const;
  virtual size_t tlab_capacity(Thread *thr) const;
  virtual size_t tlab_used(Thread *thr) const;
  virtual size_t unsafe_max_tlab_alloc(Thread *thr) const;

  virtual HeapWord* allocate(size_t size);
  virtual HeapWord* cas_allocate(size_t size);

  // Helper methods for scavenging
  virtual HeapWord* top_region_for_stripe(HeapWord* stripe_start) {
    int index = grp_index_contains_obj(stripe_start);
    assert(index > -1, "There's no region containing the stripe");
    return _collections->at(index)->space()->top();
  }

  int grp_index_contains_obj(const void* p) const {
    for (int i = 0; i < _collections->length(); ++i) {
      if(_collections->at(i)->space()->contains((HeapWord*)p))
        return i;
    }
    return -1;
  }

  virtual void      clear(bool mangle_space);
  virtual void      set_top(HeapWord* value);
  virtual void      set_end(HeapWord* value) { _end = value; }

  // Iteration.
  virtual void oop_iterate(ExtendedOopClosure* cl);
  virtual void oop_iterate_no_header(OopClosure* cl);
  virtual void object_iterate(ObjectClosure* cl);

  // Debugging virtuals
  virtual void print_on(outputStream* st) const;
  virtual void print_short_on(outputStream* st) const;
  virtual void verify();

  // Debugging non-virtual
  void print_current_space_layout(bool descriptive, bool only_collections);
};

#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_BDCMUTABLESPACE_HPP

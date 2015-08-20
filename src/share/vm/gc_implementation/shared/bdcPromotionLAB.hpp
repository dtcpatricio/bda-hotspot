#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_BDCPROMOTIONLAB_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_BDCPROMOTIONLAB_HPP

#include "gc_implementation/shared/bdaGlobals.hpp"
#include "gc_implementation/parallelScavenge/objectStartArray.hpp"
#include "gc_interface/collectedHeap.inline.hpp"
#include "memory/allocation.hpp"

//
// The BDCPromotionLAB classes are the PLABs that provide big-data bloated
// collections (HashMaps, HashSets, etc) specific placement during the
// promotion.
// It is very similar to a MutableSpace. I have thought on making it a subclass
// of BDCMutableSpace, since there is already an implementation for the
// separation of spaces. However, the MutableSpace class contains too many
// methods and it is a different abstraction. Therefore, this would have a lot of
// useless stuff. If, in the future there is need for it, it is quite possible
// and trivial to merge. This object is held by each GC Thread,
// thus there is safety knowing that each one addresses a different space.
//

class BDCPromotionLAB : public CHeapObj<mtGC> {
  friend class VMStructs;

protected:

  class PLABRegion : public CHeapObj<mtGC> {
    BDACollectionType _colltype;

    HeapWord *_top;
    HeapWord *_bottom;
    HeapWord *_end;

  public:
    PLABRegion(BDACollectionType ct) : _colltype(ct) { }
    ~PLABRegion() { }

    void initialize(MemRegion mr) {
      _bottom = mr.start();
      _top = mr.start();
      _end = mr.end();
    }

    // Accessors
    HeapWord* top() const { return _top; }
    HeapWord* end() const { return _end; }
    HeapWord* bottom() const { return _bottom; }
    BDACollectionType type() const { return _colltype; }

    // Setters
    void set_top(HeapWord* value) { _top = value; }
    void set_end(HeapWord* value) { _end = value; }
    void set_bottom(HeapWord* value) { _bottom = value; }

    // Helper methods
    static bool equals(void* region_type, PLABRegion* r) {
      return *(BDACollectionType*)region_type == r->type();
    }

    bool contains(HeapWord *p) { return p >= _bottom && p < _end; }
  };

private:
  GrowableArray<PLABRegion*>* _collections;
  size_t _page_size;

protected:
  static size_t filler_header_size;

  enum LabState {
    needs_flush,
    flushed,
    zero_size
  };

  // this pointers are to keep the space controlled
  HeapWord* _top;
  HeapWord* _bottom;
  HeapWord* _end;
  LabState _state;

  void set_top(HeapWord* value)    { _top = value; }
  void set_bottom(HeapWord* value) { _bottom = value; }
  void set_end(HeapWord* value)    { _end = value; }

  void select_limits(MemRegion mr, HeapWord** start, HeapWord** tail);

  BDCPromotionLAB();

public:

  void initialize(MemRegion lab);

  virtual void flush();

  HeapWord* bottom() const                       { return _bottom; }
  HeapWord* end() const                          { return _end; }
  HeapWord* top() const                          { return _top; }
  size_t page_size() const                       { return _page_size; }
  GrowableArray<PLABRegion*>* collections() const { return _collections; }

  bool is_flushed()        { return _state == flushed; }

  bool unallocate_object(HeapWord* obj, size_t obj_size);

  // Return a subregion containing all objects in this space
  MemRegion used_region(BDACollectionType type); // collection type version
  MemRegion used_region() { return MemRegion(bottom(), top()); }

  bool contains(const void* p) const { return p >= _bottom && p < _end; }

  int grp_index_contains_obj(const void* p) const {
    for(int i = 0; i < collections()->length(); ++i) {
      if(collections()->at(i)->contains((HeapWord*)p))
        return i;
    }
    return -1;
  }

  // Size computations, both for all regions or for some specific ones
  size_t capacity() const            { return byte_size(bottom(), end()); }
  size_t used() const                { return byte_size(bottom(), top()); }
  size_t free() const                { return byte_size(top(),    end()); }

  size_t capacity(BDACollectionType type) const;
  size_t used(BDACollectionType type) const;
  size_t free(BDACollectionType type) const;
};

class BDCYoungPromotionLAB : public BDCPromotionLAB {

public:

  BDCYoungPromotionLAB() { }

  inline HeapWord* allocate(size_t size);

};

class BDCOldPromotionLAB : public BDCPromotionLAB {

private:
  ObjectStartArray* _start_array;

public:
  BDCOldPromotionLAB() : _start_array(NULL) { }
  BDCOldPromotionLAB(ObjectStartArray* start_array) : _start_array(start_array) { }

  void set_start_array(ObjectStartArray* start_array) { _start_array = start_array; }

  virtual void flush();
  HeapWord* allocate(size_t size);
};

#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_BDCPROMOTIONLAB_HPP

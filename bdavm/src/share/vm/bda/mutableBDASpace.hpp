#ifndef SHARE_VM_BDA_MUTABLEBDASPACE_HPP
#define SHARE_VM_BDA_MUTABLEBDASPACE_HPP

# include "bda/bdaGlobals.hpp"
# include "bda/gen_queue.hpp"
# include "gc_implementation/shared/mutableSpace.hpp"
# include "gc_implementation/parallelScavenge/objectStartArray.hpp"
# include "gc_implementation/parallelScavenge/parMarkBitMap.hpp"
# include "runtime/prefetch.inline.hpp"


// Implementation of an allocation space for big-data collection placement


// Forward declarations
class Thread;
class CollectedHeap;
class SpaceDecorator;
class ObjectStartArray;

//
// The MutableBDASpace class is a general object that encapsulates multiple
// CGRPSpaces. It is implemented in a similar fashion as MutableNUMASpace.
//
class MutableBDASpace : public MutableSpace
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

  static const size_t Log2BlockSize;
  static const size_t BlockSize;
  static const size_t BlockSizeBytes;

  static size_t       _filler_header_size;

  // This class wraps the addressable space of the MutableBDASpace
  // for a particular collection type, or none at all.
  class CGRPSpace : public CHeapObj<mtGC>
  {
    friend class MutableBDASpace;
    
    enum { CONTAINER_IN_POOL_MASK = 1 };
    
    MutableSpace *                 _space;
    BDARegion *                    _type;
    // Containers currently in use by the space. They may be at least partly occupied.
    // If their contents can be merged onto another container of the same family, then
    // they shall but only at OldGC, i.e., only when the old space is actually scanned.
    GenQueue<container_t, mtGC> * _containers;
    // A pool of containers already allocated and ready
    // to be assigned to a space, i.e., it works as a cache of free containers in order
    // to avoid free/malloc of new ones. It is implemented as a queue that wether fills
    // or empties, during the final OldGC phase and during Young GC, respectively.
    GenQueue<container_t, mtGC> * _pool;
    // Pool of large containers already allocated and ready to be assign to a space.
    GenQueue<container_t, mtGC> * _large_pool;
    
    // GC support
    container_t                  _gc_current;

    // A pointer to the parent
    MutableBDASpace *              _manager;

#ifdef ASSERT
    // Stats fields:
    //  Number of segments allocated or returned from the pool in the last gc
    int _segments_since_last_gc;
#endif
    

    // Helper function to calculate the power of base over exponent using bit-wise
    // operations. It is inlined for such.
    static inline int    power_function(int base, int exp);
    // Calculates a container segment size, allocates one in the space and initializes it.
    // It allocates both containers with parent object and segments with children object only.
    container_t allocate_container(size_t size);
    // Also calculates a container segment size, but only based on the size_t
    container_t allocate_large_container(size_t size);
    // Masks containers by ORing the CONTAINER_IN_POOL_MASK on the _start field of the struct
    // Any subsequent use must unmask the container because an ORed _start is invalid since
    // containers/segments are aligned byte aligned.
    inline void mask_container(container_t& c );
    inline void unmask_container(container_t& c );


   public:

    // This are globals for CGRPSpace so they can be updated with the result
    // of heuristics that compute the use of the bda-spaces by the application.
    static int dnf;
    static int delegation_level;
    static int default_collection_size;
    static int node_fields;
    // This value is kept since it is the same for every regular segment.
    static size_t segment_sz;

    CGRPSpace(size_t alignment, BDARegion * region, MutableBDASpace * manager) :
      _type(region), _manager(manager) {
      _space = new MutableSpace(alignment);
      _containers = GenQueue<container_t, mtGC>::create();
      _pool = GenQueue<container_t, mtGC>::create();
      _large_pool = GenQueue<container_t, mtGC>::create();
#ifdef ASSERT
      _segments_since_last_gc = 0;
#endif
    }
    ~CGRPSpace() {
      delete _space;
      for (GenQueueIterator<container_t, mtGC> iterator = _containers->iterator();
           *iterator != NULL;
           ++iterator) {
        container_t c = *iterator;
        FreeHeap((void*)c, mtGC);
      }
      for (GenQueueIterator<container_t, mtGC> iterator = _pool->iterator();
           *iterator != NULL;
           ++iterator) {
        container_t c = *iterator;
        FreeHeap((void*)c, mtGC);
      }
      for (GenQueueIterator<container_t, mtGC> iterator = _large_pool->iterator();
           *iterator != NULL;
           ++iterator) {
        container_t c = *iterator;
        FreeHeap((void*)c, mtGC);
      }
    }

    static bool equals(void* container_type, CGRPSpace* s) {
      return (BDARegion*)container_type == s->container_type();
    }

    BDARegion *      container_type()  const { return _type; }
    MutableSpace *   space()           const { return _space; }
    int              container_count() const { return _containers->n_elements(); }
    
    // This is called for new collections, i.e., that need a parent container
    inline container_t   push_container(size_t size);
    // This is called for already existing collections when they need a new segment
    inline HeapWord *    allocate_new_segment(size_t size, container_t& c);
    // This is called to calculate the segment size based on the user's launch parameters
    static inline size_t calculate_reserved_sz();
    // This is called to calculate a large segment size for large arrays. It bumps size
    // to the MinRegionSize in order to reserved the most possible.
    inline size_t        calculate_large_reserved_sz(size_t size);
    // Allocates the reserved_sz to the space and sets the appropriate pointers
    inline container_t   install_container_in_space(size_t reserved_sz, size_t size);

    // Destructors --- these should only be called for the other's space, since for the rest
    // the leftover segments are to be pushed to the pool for reuse.
    inline bool clear_delete_containers();    
    
    // This is called during the final stage of OldGC when free segments are returned to the pool
    inline void add_to_pool(container_t c);
    inline bool not_in_pool(container_t c);
    
    // GC support
    inline container_t cas_get_next_container();
    inline container_t get_container_with_addr(HeapWord * addr);
    inline void        save_top_ptrs();
    inline void        set_shared_gc_pointer() { _gc_current = _containers->peek(); }

    // Size computations (in heapwords and bytes)
    inline size_t used_in_words() const;
    inline size_t free_in_words() const;
    inline size_t used_in_bytes() const;
    inline size_t free_in_bytes() const;
    
    // Iteration support
    void object_iterate_containers(ObjectClosure * cl);
    void oop_iterate_containers(ExtendedOopClosure * cl);
    void oop_iterate_no_header_containers(OopClosure * cl);

    // To verify oops
    void verify();
    
    // Statistics and printing
    void print_container_fragmentation_stats() const;
    void print_allocation(container_t c, bool large = false) const;
    void print_allocation_stats(outputStream * st) const;
    void print_used(outputStream * st) const;

#ifdef ASSERT
    // Reset stats fields
    void reset_stats();
#endif
  };

 private:
  GrowableArray<CGRPSpace*>* _spaces;
  ParMarkBitMap              _segment_bitmap;
  size_t                     _page_size;

 protected:

  static ObjectStartArray *  _start_array;
  
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

  // Marking of bits in the container segment bitmap
  inline bool mark_container(container_t c);
  inline void allocate_block(HeapWord * obj);
  
 public:

  MutableBDASpace(size_t alignment, ObjectStartArray * start_array);
  virtual ~MutableBDASpace();
  virtual void      initialize(MemRegion mr,
                               bool clear_space,
                               bool mangle_space,
                               bool setup_pages = SetupPages);

  bool                       post_initialize();
  void                       set_page_size(size_t page_size) { _page_size = page_size; }
  size_t                     page_size() const { return _page_size; }
  GrowableArray<CGRPSpace*>* spaces() const { return _spaces; }
  ParMarkBitMap const *      segment_bitmap() const { return &_segment_bitmap; }

  container_t container_for_addr(HeapWord * addr);
  void          add_to_pool(container_t c, uint id);
  inline void   set_shared_gc_pointers();
  inline void   save_tops_for_scavenge();

  // Statistics functions
  float avg_nsegments_in_bda();
  
  

  // Boolean queries - the others are already implemented on mutableSpace.hpp
  bool contains(const void* p) const {
    assert(spaces() != NULL, "_Spaces array no initialized");
    for(int i = 0; i < spaces()->length(); ++i) {
      if(spaces()->at(i)->space()->contains((HeapWord*)p))
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

  // Accessors to spaces
  MutableSpace * non_bda_space() const { return non_bda_grp()->space(); }
  CGRPSpace    * non_bda_grp  () const { return _spaces->at(0); }
  MutableSpace * region_for(BDARegion* region) const {
    int i = _spaces->find(&region, CGRPSpace::equals);
    return _spaces->at(i)->space();
  }

  virtual HeapWord *top_specific(BDARegion* type) {
    int i = _spaces->find(&type, CGRPSpace::equals);
    return _spaces->at(i)->space()->top();
  }
  virtual MemRegion used_region(BDARegion* type) {
    int i = _spaces->find(&type, CGRPSpace::equals);
    return _spaces->at(i)->space()->used_region();
  }
  virtual int num_bda_regions() { return _spaces->length() - 1; }

  inline  int  container_count();
  inline  bool is_bdaspace_empty();

  // Selection of bits for the beginning and ending of container segments, respectively
  inline HeapWord * get_next_beg_seg(HeapWord * beg, HeapWord * end) const;
  inline HeapWord * get_next_end_seg(HeapWord * beg, HeapWord * end) const;

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

  // Allocation methods
  virtual HeapWord* allocate(size_t size);
  virtual HeapWord* cas_allocate(size_t size);
  container_t     allocate_container (size_t size, BDARegion * r);
  // This version updates the container with a new one if a new segment was needed.
  HeapWord*         allocate_element(size_t size, container_t& r);

  // Helper methods for scavenging
  virtual HeapWord* top_region_for_stripe(HeapWord* stripe_start) {
    int index = grp_index_contains_obj(stripe_start);
    assert(index > -1, "There's no region containing the stripe");
    return _spaces->at(index)->space()->top();
  }

  int grp_index_contains_obj(const void* p) const {
    for (int i = 0; i < _spaces->length(); ++i) {
      if(_spaces->at(i)->space()->contains((HeapWord*)p))
        return i;
    }
    return -1;
  }

  // Clear and reset methods
  void         clear_delete_containers_in_space(uint space_id);
  void         reset_grp_stats();     
  virtual void clear(bool mangle_space);

  // Setters
  virtual void set_top(HeapWord* value);
  virtual void set_end(HeapWord* value) { _end = value; }

  // Iteration.
  virtual void oop_iterate(ExtendedOopClosure* cl);
  virtual void oop_iterate_no_header(OopClosure* cl);
  virtual void object_iterate(ObjectClosure* cl);

  // Debugging virtuals
  virtual void print_on(outputStream* st) const;
  virtual void print_short_on(outputStream* st) const;
  virtual void verify();

  // Debugging non-virtual
  void print_object_space() const;
  void print_spaces_fragmentation_stats() const;
#ifdef ASSERT
  void print_allocation_stats() const;
#endif
};

#endif // SHARE_VM_BDA_MUTABLEBDASPACE_HPP

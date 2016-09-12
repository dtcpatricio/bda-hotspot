#ifndef SHARE_VM_BDA_REFQUEUE_HPP
#define SHARE_VM_BDA_REFQUEUE_HPP

# include "memory/allocation.hpp"
# include "oops/oopsHierarchy.hpp"
# include "bda/bdaGlobals.hpp"
# include "runtime/atomic.inline.hpp"
# include "utilities/taskqueue.hpp"

class Ref : public CHeapObj<mtGC> {

 private:
  Ref *       _next;
  oop         _actual_ref;
  BDARegion * _region;

 public:

  Ref(oop obj, BDARegion* r);

  Ref * next()            { return _next; }
  void set_next(Ref* ref) { _next = ref; }

  oop operator () ()   const { return _actual_ref; }
  oop *       ref()          { return &_actual_ref; }
  BDARegion * region() const { return _region; }
};

class RefQueue : public CHeapObj<mtGC> {

 private:
  Ref * _insert_end;
  Ref * _remove_end;
  DEBUG_ONLY(int _n_elements;)

 public:

  enum RefType {
    element = 0,
    container = 1
  };
  
  // Factory methods
  static RefQueue * create();

  // Methods for modification of the queue
  inline void  enqueue(oop obj, BDARegion * r);
  inline Ref * dequeue();

  inline void set_insert_end(Ref* ref) { _insert_end = ref; }
  inline void set_remove_end(Ref* ref) { _remove_end = ref; }
  inline Ref * insert_end() { return _insert_end; }
  inline Ref * remove_end() { return _remove_end; }
};

// Inline definitions

inline void
RefQueue::enqueue(oop obj, BDARegion * r)
{
  Ref * ref = new Ref(obj, r);
  Ref * temp = NULL;
  do {
    temp = insert_end();
    ref->set_next(temp);
  } while(Atomic::cmpxchg_ptr(ref,
                              &_insert_end,
                              temp) != temp);
  if(remove_end() == NULL) set_remove_end(ref);
  DEBUG_ONLY(Atomic::inc(&_n_elements);)
}

inline Ref*
RefQueue::dequeue()
{
  Ref * temp = NULL;
  Ref * next = NULL;
  do {
    temp = remove_end();
    next = temp->next();
  } while(Atomic::cmpxchg_ptr(next,
                              &_remove_end,
                              temp) != temp);
  if(remove_end() == NULL) set_insert_end(NULL);
  DEBUG_ONLY(Atomic::dec(&_n_elements);)
  return temp;
}

/*
 * RefStack
 * The RefStack is a stack whose elements are to group bda container refs
 * (i.e. elements of a collection) with the container they belong to. It works much as a
 * StarTask (see taskqueue.hpp) but it groups the oop with the container it belongs to in the
 * bda-spaces to inform the collector where the oop should be promoted.
 */
class BDARefTask {

  void *        _holder;
  container_t * _container;

  enum { COMPRESSED_OOP_MASK = 1 };
  
 public:
  
  BDARefTask(narrowOop * p, container_t * c) : _container(c) {
    assert(((uintptr_t)p & COMPRESSED_OOP_MASK) == 0, "Information loss!");
    _holder = (void*)((uintptr_t)p | COMPRESSED_OOP_MASK);
  }
  BDARefTask(oop* p, container_t * c) : _container(c) {
    assert(((uintptr_t)p & COMPRESSED_OOP_MASK) == 0, "Information loss!");
    _holder = (void*)p;
  }
  BDARefTask()                 { _holder = NULL; _container = NULL; }
  operator oop * ()            { return (oop*)_holder; }
  operator narrowOop * ()      { return (narrowOop*)((uintptr_t)_holder & ~COMPRESSED_OOP_MASK); }  
  container_t * operator -> () { return _container; }  

  bool is_narrow() const {
    return (((uintptr_t)_holder & COMPRESSED_OOP_MASK) != 0);
  }
};

typedef Stack<BDARefTask, mtGC> BDARefStack;
#endif // SHARE_VM_BDA_REFQUEUE_HPP

#ifndef SHARE_VM_BDA_REFQUEUE_HPP
#define SHARE_VM_BDA_REFQUEUE_HPP

# include "memory/allocation.hpp"
# include "oops/oopsHierarchy.hpp"
# include "bda/bdaGlobals.hpp"
# include "runtime/atomic.inline.hpp"

class Ref : public CHeapObj<mtGC> {

 private:
  Ref *       _next;
  oop         _actual_ref;
  BDARegion * _region;

 public:

  Ref(oop obj, BDARegion* r);

  Ref * next()            { return _next; }
  void set_next(Ref* ref) { _next = ref; }
};

class RefQueue : public CHeapObj<mtGC> {

 private:
  Ref * _insert_end;
  Ref * _remove_end;
  DEBUG_ONLY(int _n_elements;)

 public:
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
#endif // SHARE_VM_BDA_REFQUEUE_HPP

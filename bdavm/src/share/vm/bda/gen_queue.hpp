#ifndef SHARE_VM_BDA_GEN_QUEUE_HPP
#define SHARE_VM_BDA_GEN_QUEUE_HPP

# include "memory/allocation.hpp"
# include "runtime/atomic.inline.hpp"
# include "runtime/mutex.hpp"
# include "runtime/mutexLocker.hpp"

// forward declaration
template <class E, MEMFLAGS F> class GenQueueIterator;

template <class E, MEMFLAGS F>
class GenQueue : public CHeapObj<F> {

  friend class GenQueueIterator<E, F>;
  
 private:

  Monitor * _monitor;
  E _insert_end;
  E _remove_end;
  int _n_elements;

 public:

  static GenQueue * create();
  // The caller is responsible for deallocating the queue!
  // This only clears the ends of the queue.
  static void       destroy(GenQueue<E, F> * queue);

  inline void enqueue(E el);
  inline E    dequeue();
  inline E    peek()        const { return _remove_end; }
  inline int  n_elements() const { return _n_elements; }
  inline void remove_element(E el);
  inline void remove_element_mt(E el);
  inline void phantom_remove();

  GenQueueIterator<E, F> iterator() const;

 protected:

  inline void set_insert_end(E el) { _insert_end = el; }
  inline void set_remove_end(E el) { _remove_end = el; }
  
  inline E    insert_end() const     { return _insert_end; }
  inline E    remove_end() const     { return _remove_end; }

  Monitor * monitor() { return _monitor; }
};

template <class E, MEMFLAGS F>
inline void
GenQueue<E, F>::enqueue(E el)
{
  E temp = NULL;
  do {
    temp = insert_end();
  } while (Atomic::cmpxchg_ptr(el,
                               &_insert_end,
                               temp) != temp);
  el->_previous = temp;
  if (temp != NULL) temp->_next = el;
  if (el->_previous == NULL && remove_end() == NULL) set_remove_end(el);
  Atomic::inc(&_n_elements);
  assert (temp == NULL || temp->_next == el, "just checking if assign was valid");
}

template <class E, MEMFLAGS F>
inline E
GenQueue<E, F>::dequeue()
{
  E temp = NULL;
  E next = NULL;
  do {
    temp = remove_end();
    if (temp == NULL)
      return temp;
    next = temp->_next;
  } while (Atomic::cmpxchg_ptr(next,
                               &_remove_end,
                               temp) != temp);
  if(next != NULL) next->_previous = NULL;
  if(remove_end() == NULL) set_insert_end(NULL);
  // A removed element should not have its fields associated with the queue
  // - _previous should be NULL by now (assert?).
  temp->_next = NULL;
  Atomic::dec(&_n_elements);
  return temp;
}

// This function attempts to remove the element from the insert_end
// or from the remove_end. If this element is not in any of those,
// then _previous/_next pointers in the neighbor elements are set.
// NOT MT SAFE!! (currently, only post_compact() uses this function,
// through add_to_pool() method of CGRPSpace).
template <class E, MEMFLAGS F>
inline void
GenQueue<E, F>::remove_element(E el)
{
  if (remove_end() == el) {
    dequeue();
  } else if (insert_end() == el) {
    set_insert_end(el->_previous);
    insert_end()->_next = NULL;
  } else {
    el->_previous->_next = el->_next;
    el->_next->_previous = el->_previous;
  }

  // If this element is a parent container then promote one of its segments to parent.
  // If this new parent is empty then it shall be removed later.  
  if (el->_prev_segment == NULL) {
    if (el->_next_segment != NULL) {
      el->_next_segment->_prev_segment = NULL; // this segment is now parent
    } else {
      // Do nothing, just fall through.
    }
  } else {
    // This is a segment in the middle of its brothers.
    el->_prev_segment->_next_segment = el->_next_segment;
    if (el->_next_segment != NULL)
      el->_next_segment->_prev_segment = el->_prev_segment;
  }

  // A removed element should not have its fields pointing anywhere else
  el->_next = NULL; el->_previous = NULL;
  Atomic::dec(&_n_elements);
}

// This function attempts to remove the element from the queue/list.
// It uses the monitor() lock to prevent conflicting changes.
template <class E, MEMFLAGS F>
inline void
GenQueue<E, F>::remove_element_mt(E el)
{
  {
    MutexLockerEx ml (monitor(), Mutex::_no_safepoint_check_flag);
    remove_element (el);
  }
}

// This function does not remove an element, per itself, but
// just reduces the number of elements in the queue. This is enough
// to remove the elements in a later stage and requires less computation.
template <class E, MEMFLAGS F>
inline void
GenQueue<E, F>::phantom_remove()
{
  Atomic::dec(&_n_elements);
}


template <class E, MEMFLAGS F>
GenQueue<E, F> *
GenQueue<E, F>::create()
{
  GenQueue * queue = new GenQueue<E, F>();
  Monitor  * monitor = new Monitor(Mutex::nonleaf+3, "GenQueue lock");
  queue->set_insert_end(NULL);
  queue->set_remove_end(NULL);
  queue->_n_elements = 0;
  queue->_monitor = monitor;
  return queue;
}

template <class E, MEMFLAGS F>
void
GenQueue<E, F>::destroy(GenQueue<E, F> * queue)
{
  queue->set_insert_end(NULL);
  queue->set_remove_end(NULL);
  queue->_n_elements = 0;
}

template <class E, MEMFLAGS F>
GenQueueIterator<E, F>
GenQueue<E, F>::iterator() const 
{
  return GenQueueIterator<E, F>(this);
}


template <class E, MEMFLAGS F>
class GenQueueIterator : public StackObj {

  friend class GenQueue<E, F>;
  
 private:

  const GenQueue<E, F>* _queue;
  E                     _current_element;

  GenQueueIterator(const GenQueue<E, F>* queue) :
    _queue(queue)
    {
      assert (queue->remove_end() != NULL && queue->insert_end() != NULL,
              "queue is empty or malformed");
      _current_element = _queue->remove_end();
    }

 public:
  GenQueueIterator<E, F>& operator++()  { _current_element = _current_element->_next; return *this; }
  E                       operator* ()  { return _current_element; }  
};

#endif // SHARE_VM_BDA_GEN_QUEUE_HPP

#ifndef SHARE_VM_BDA_GEN_QUEUE_HPP
#define SHARE_VM_BDA_GEN_QUEUE_HPP

# include "memory/allocation.hpp"
# include "runtime/atomic.inline.hpp"

// forward declaration
template <class E, MEMFLAGS F> class GenQueueIterator;

template <class E, MEMFLAGS F>
class GenQueue : public CHeapObj<F> {

  friend class GenQueueIterator<E, F>;
  
 private:

  E _insert_end;
  E _remove_end;
  int _n_elements;

 public:

  static GenQueue * create();

  inline void enqueue(E el);
  inline E    dequeue();
  inline E    peek()        const { return _remove_end; }
  inline int  n_elements() const { return _n_elements; }
  inline void remove_element(E el);

  GenQueueIterator<E, F> iterator() const;

 protected:

  inline void set_insert_end(E el) { _insert_end = el; }
  inline void set_remove_end(E el) { _remove_end = el; }
  
  inline E    insert_end() const     { return _insert_end; }
  inline E    remove_end() const     { return _remove_end; }
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
  if (remove_end() == NULL) set_remove_end(el);
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
  Atomic::dec(&_n_elements);
  return temp;
}

// This function attempts to remove the element from the insert_end
// or from the remove_end. If this element is not in any of those,
// then _previous/_next pointers in the neighbor elements are set.
// NOT MT SAFE!! (currently, only post_compact() uses this function,
// though add_to_pool() method of CGRPSpace).
template <class E, MEMFLAGS F>
inline void
GenQueue<E,F>::remove_element(E el)
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

  // If this element is a parent container then merge its segments to another's.
  // p is 'previous' and p_s is 'previous's segment'
  if (el->_prev_segment == NULL) {
    if (el->_next_segment != NULL) {
      E p, p_s;
      if (el->_previous != NULL) {
        p = el->_previous;
      } else {
        p = el->_next;
      }
      p_s = p;
      while ( (p_s = p_s->_next_segment) != NULL ) {
        p = p_s;
      }
      el->_next_segment->_prev_segment = p;
      p->_next_segment = el->_next_segment;
    } else {
      // Do nothing, this is a lone segment and it has already been removed.
    }
  } else {
    // This is a segment in the middle of its brothers.
    el->_prev_segment->_next_segment = el->_next_segment;
    if (el->_next_segment != NULL)
      el->_next_segment->_prev_segment = el->_prev_segment;
  }

  Atomic::dec(&_n_elements);
}


template <class E, MEMFLAGS F>
GenQueue<E, F> *
GenQueue<E, F>::create()
{
  GenQueue * queue = new GenQueue<E, F>();
  queue->set_insert_end(NULL);
  queue->set_remove_end(NULL);
  queue->_n_elements = 0;
  return queue;
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

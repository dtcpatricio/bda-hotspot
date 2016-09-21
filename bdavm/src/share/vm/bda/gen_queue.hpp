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
  inline E   dequeue();
  inline E   peek() { return _remove_end; }

  GenQueueIterator<E, F> iterator() const;

 protected:

  inline void set_insert_end(E el) { _insert_end = el; }
  inline void set_remove_end(E el) { _remove_end = el; }
  inline int  n_elements() const     { return _n_elements; }
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
  if (temp != NULL) temp->_next = el;
  if (remove_end() == NULL) set_remove_end(el);
  Atomic::inc(&_n_elements);
}

template <class E, MEMFLAGS F>
inline E
GenQueue<E, F>::dequeue()
{
  E temp = NULL;
  E next = NULL;
  do {
    temp = remove_end();
    next = temp->_next;
  } while (Atomic::cmpxchg_ptr(next,
                               &_remove_end,
                               temp) != NULL);
  if(remove_end() == NULL) set_insert_end(NULL);
  Atomic::dec(&_n_elements);
  return temp;
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

#ifndef SHARE_VM_BDA_TASKQUEUE_HPP
#define SHARE_VM_BDA_TASKQUEUE_HPP

# include "utilities/taskqueue.hpp"
# include "bda/bdaGlobals.hpp"
# include "bda/mutableBDASpace.hpp"

template<class E, MEMFLAGS F, unsigned int N = TASKQUEUE_SIZE>
class ContainerOverflowTaskQueue : public OverflowTaskQueue<E, F, N>
{

  container_t* _collection;

 public:

  typedef MutableBDASpace::CGRPSpace * space_t;

  inline void push_container(size_t size, space_t s);
};

template<class E, MEMFLAGS F, unsigned int N>
inline void
ContainerOverflowTaskQueue<E, F, N>::push_container(size_t size, space_t s)
{
  _collection = s->push_container(size);
}

template<class T, MEMFLAGS F>
class ContainerTaskQueueSet : public GenericTaskQueueSet<T, F> {


 public:

  typedef MutableBDASpace * oldspace_t;

  ContainerTaskQueueSet(int n, oldspace_t s) :
    GenericTaskQueueSet<T, F>(n)
    { _space = s; }

 private:

  oldspace_t _space;
};

#endif // SHARE_VM_BDA_TASKQUEUE_HPP

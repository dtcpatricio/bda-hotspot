# include "bda/refqueue.hpp"

Ref::Ref(oop obj, BDARegion * r) : _next(NULL),
                                   _actual_ref(obj),
                                   _region(r)
{
}

RefQueue*
RefQueue::create()
{
  RefQueue * queue = new RefQueue();
  queue->set_insert_end(NULL);
  queue->set_remove_end(NULL);
  return queue;
}

import java.lang.Runnable;

import java.util.Iterator;
import java.util.List;
import java.util.LinkedList;
import java.util.Map;

public class MapThread implements Runnable
{
  private List<MyHashMap<Long, Value>> dataList;
  private MyHashMap<Long, List<Pair<Value, Value>>> sharedKeysMap;
  private MyHashMap<Long, Value>             unsharedKeysMap;
  private int                                threadId;
  private int                                sharedColor;
  private int                                unsharedColor;

  public int                                       getThreadId           () { return threadId; }
  public MyHashMap<Long, List<Pair<Value, Value>>> getReducedSharedMap   () { return sharedKeysMap; }
  public MyHashMap<Long, Value>                    getReducedUnsharedMap () { return unsharedKeysMap; }
  
  public MapThread (List<MyHashMap<Long, Value>> list,
                    int id, int sMaps, int sColor, int uColor)
    {
      dataList = list;
      threadId = id;
      sharedColor = sColor;
      unsharedColor = uColor;
      unsharedKeysMap = new MyHashMap<Long, Value>(sMaps / 100, uColor);
      sharedKeysMap = new MyHashMap<Long, List<Pair<Value, Value>>>(sMaps / 100, sColor);
    }

  public void run ()
    {
      while (!dataList.isEmpty()) {
        MyHashMap<Long, Value> map = dataList.remove(0);
        Iterator it = map.entrySet().iterator();
        while (it.hasNext()) {
          Map.Entry<Long, Value> entry = (Map.Entry<Long, Value>)it.next();
          boolean success = findMatchAndRemove(entry);
          if (!success) {
            unsharedKeysMap.put(entry.getKey(), entry.getValue());
          }
          it.remove();
        }
      }
    }
  
  private boolean findMatchAndRemove(Map.Entry<Long, Value> e)
    {
      boolean ret = false;
      for (int i = 0; i < dataList.size(); ++i) {
        MyHashMap<Long, Value> m = dataList.get(i);
        if (m.containsKey(e.getKey())) {
          Value mValue = m.get(e.getKey());
          Value eValue = e.getValue();
          Pair<Value, Value> p = new Pair (mValue, eValue);
          List<Pair<Value, Value>> lst = sharedKeysMap.get(e.getKey());
          if (lst != null) {
            lst.add(p);
          } else {
            lst = new LinkedList<Pair<Value, Value>>();
            lst.add(p);
            sharedKeysMap.put(e.getKey(), lst);
          }
          m.remove(e.getKey());
          ret = true;
        }
      }
      return ret;
    }
}


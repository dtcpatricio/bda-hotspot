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
  // Statistics stuff
  private long                             avgAccessTime;
  private int                                nElementsAccessed;
  private long                             avgWriteTime;
  private int                                nElementsWritten;

  public int                                       getThreadId           () { return threadId; }
  public MyHashMap<Long, List<Pair<Value, Value>>> getReducedSharedMap   () { return sharedKeysMap; }
  public MyHashMap<Long, Value>                    getReducedUnsharedMap () { return unsharedKeysMap; }
  public long                                    getAvgAccessTime      () { return avgAccessTime;}
  public long                                    getAvgWriteTime       () { return avgWriteTime;}
  
  public MapThread (List<MyHashMap<Long, Value>> list,
                    int id, int sMaps, int sColor, int uColor)
    {
      dataList = list;
      threadId = id;
      sharedColor = sColor;
      unsharedColor = uColor;
      this.avgAccessTime = 0;
      this.avgWriteTime = 0;
      this.nElementsAccessed = 0;
      this.nElementsWritten = 0;
      unsharedKeysMap = new MyHashMap<Long, Value>(sMaps / 100, uColor);
      sharedKeysMap = new MyHashMap<Long, List<Pair<Value, Value>>>(sMaps / 100, sColor);
    }

  public void run ()
    {
      while (!dataList.isEmpty()) {
        MyHashMap<Long, Value> map = dataList.remove(0);
        Iterator it = map.entrySet().iterator();
        while (it.hasNext()) {
          // start clock and get elem
          long start = System.nanoTime();
          Map.Entry<Long, Value> entry = (Map.Entry<Long, Value>)it.next();
          long elapsed = System.nanoTime() - start;
          calculateAvgAccessTime(elapsed);
          // continue
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
          // Time and get block
          long start = System.nanoTime();
          Value mValue = m.get(e.getKey());
          long elapsed = System.nanoTime() - start;
          calculateAvgAccessTime(elapsed);
          // Time and get block
          start = System.nanoTime();
          Value eValue = e.getValue();
          elapsed = System.nanoTime() - start;
          calculateAvgAccessTime(elapsed);

          Pair<Value, Value> p = new Pair (mValue, eValue);

          // Time and get block
          start = System.nanoTime();
          List<Pair<Value, Value>> lst = sharedKeysMap.get(e.getKey());
          elapsed = System.nanoTime() - start;
          calculateAvgAccessTime(elapsed);
          
          if (lst != null) {
            lst.add(p);
          } else {
            lst = new LinkedList<Pair<Value, Value>>();
            lst.add(p);
            // Time and write block
            start = System.nanoTime();
            sharedKeysMap.put(e.getKey(), lst);
            elapsed = System.nanoTime() - start;
            calculateAvgWriteTime(elapsed);
          }
          // Time and get block
          start = System.nanoTime();
          m.remove(e.getKey());
          elapsed = System.nanoTime() - start;
          calculateAvgAccessTime(elapsed);
          ret = true;
        }
      }
      return ret;
    }

  private void calculateAvgAccessTime(long elapsed)
    {
      nElementsAccessed++;
      long delta = elapsed - avgAccessTime;
      avgAccessTime += delta / nElementsAccessed;
    }
  
  private void calculateAvgWriteTime(long elapsed)
    {
      nElementsWritten++;
      long delta = elapsed - avgWriteTime;
      avgWriteTime += delta / nElementsWritten;
    }
}


import java.lang.Runnable;
import java.util.List;

public class ProcessThread implements Runnable
{
  private List<MyHashMap<Long, Value>> mapList;
  private int                          threadId;
  private int                          readCount;

  private long                         avgAccessTime;
  private int                          nElementsAccessed;

  public long getAvgAccessTime() { return avgAccessTime; }
  public int  getThreadId()      { return threadId; }
  
  public ProcessThread (List<MyHashMap<Long, Value>> list, int id, int readC)
    {
      this.avgAccessTime = 0;
      this.nElementsAccessed = 0;
      mapList = list;
      threadId = id;
      readCount = readC;
    }

  public void run () {
    for(int a = 0; a < mapList.size(); ++a) {
      MyHashMap<Long, Value> map = mapList.get(a);
      int j; int max = map.size(); int count = readCount;
      try {
        for (int i = 0; i < count; ++i) {
          // Time and get block
          long start = System.nanoTime();
          if (map.containsKey(i)) {
            long elapsed = System.nanoTime() - start;
            calculateAvgAccessTime(elapsed);

            // Time and get block
            start = System.nanoTime();
            Value v = map.get(i);
            elapsed = System.nanoTime() - start;
            calculateAvgAccessTime(elapsed);
            // Long  k = new Long(i);
            // Value n = new Value(v.value() + "scanned", v.color());
            // map.put(k, n);
          } else {
            long elapsed = System.nanoTime() - start;
            calculateAvgAccessTime(elapsed);
          }
        }
      } catch (Exception e) {
        System.err.println("(ERR) Thread " + threadId + "Caught an exception reading map " +
                           map.color());
        System.err.println(e.getCause());
        continue;
      }
      System.out.println("Thread " + threadId + " --- Read map with color " + map.color());
    }
  }

  private void calculateAvgAccessTime(long elapsed)
    {
      nElementsAccessed++;
      long delta = elapsed - avgAccessTime;
      avgAccessTime += delta / nElementsAccessed;
    }
}


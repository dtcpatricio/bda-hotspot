import java.lang.Thread;
import java.util.List;

public class ProcessThread extends Thread
{
  private List<MyHashMap<Long, Value>> mapList;
  private int                          threadId;
  private int                          readCount;
    
  public ProcessThread (List<MyHashMap<Long, Value>> list, int id, int readC)
    {
      super("Thread " + id);
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
          j = i % max;
          if (map.containsKey(j)) {
            Value v = map.get(j);
            Long  k = new Long(i);
            Value n = new Value(v.value() + "scanned", v.color());
            map.put(k, n);
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
}


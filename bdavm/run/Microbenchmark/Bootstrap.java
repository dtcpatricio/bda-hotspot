import java.util.HashMap;
import java.util.Random;
import java.util.LinkedList;
import java.util.List;

public class Bootstrap {

  // We'll have to try with Integer.MAX_VALUE, but with a bigger heap
  // to prevent OutOfMemory errors.
  public static int max = 25000;
  public static int numberMaps = 40;

  private List<MyHashMap<Long, Value>> interestingMaps;
  private List<HashMap<Long, Value>> notInterestingMaps;

    // also used as the color
    private int map_counts = 0; 

  public MyHashMap<Long, Value> getInterestingMap(int i)
    {
        if (i >= interestingMaps.size())
            return null;
        else
            return interestingMaps.get(i);
    }

  public int numberOfInterestingMaps()
    {
        return interestingMaps.size();
    }
  
  private MyHashMap<Long, Value> ConstructorHelper(int sz)
    {
      MyHashMap<Long, Value> m = new MyHashMap<Long, Value>(sz, map_counts++);
      System.out.println("\nCreated map number " + map_counts);
      return m;
  }

  public Bootstrap() {
    notInterestingMaps = new LinkedList<HashMap<Long, Value>>();
    interestingMaps    = new LinkedList<MyHashMap<Long, Value>>();
    for (int i = 0; i < numberMaps; i++) {
        MyHashMap map = ConstructorHelper(max);
        fillHashMapWithRandom(map);
        interestingMaps.add(map);
    }
  }

  private void fillHashMapWithRandom(MyHashMap<Long, Value> map) {
    if(map == null)
      return;

    // Create the "intercalator"
    HashMap<Long, Value> intercalator = new HashMap<Long, Value>(max);
    
    int color = map.color();
    Random generator = new Random((long)max);
    for(int i = 0; i < max; i++) {
      long key = generator.nextInt();
      String str = "Booh!";
      Value value = new Value(str, color);

      // Intercalator values have color -1, which should not be caught
      Long  intercalatorK = new Long(key);
      Value intercalatorV = new Value(str, -1);
      intercalator.put(intercalatorK, intercalatorV);
      
      map.put(key, value);
    }

    notInterestingMaps.add(intercalator);
  }

  private void fillHashMap(MyHashMap<Long, Value> map1,
                           MyHashMap<Long, Value> map2) {
      int color = map1.color();
      map1.putAll(map2);
      map1.put(27L, new Value("Duarte", color));
      map1.put(42L, new Value("Ivan", color));
      map1.put(27L, new Value("Candida", color)); // override
  }
}

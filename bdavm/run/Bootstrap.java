import java.util.HashMap;
import java.util.Random;
import java.util.LinkedList;
import java.util.List;

public class Bootstrap {

  // We'll have to try with Integer.MAX_VALUE, but with a bigger heap
  // to prevent OutOfMemory errors.
  public static int max = 25000;

  private MyHashMap<Long, Value> map;
  private MyHashMap<Long, Value> map1;
  private MyHashMap<Long, Value> map2;
  private MyHashMap<Long, Value> map3;
  private MyHashMap<Long, Value> map4;
  private MyHashMap<Long, Value> map5;
  private MyHashMap<Long, Value> map6;

  private List<HashMap<Long, Value>> notInterestingMaps;

    // also used as the color
    private int map_counts = 0; 

  public MyHashMap<Long, Value> getMap0() { return map; }
  public MyHashMap<Long, Value> getMap1() { return map1; }
  public MyHashMap<Long, Value> getMap2() { return map2; }
  public MyHashMap<Long, Value> getMap3() { return map3; }
  public MyHashMap<Long, Value> getMap4() { return map4; }
  public MyHashMap<Long, Value> getMap5() { return map5; }
  public MyHashMap<Long, Value> getMap6() { return map6; }
  
  
  private MyHashMap<Long, Value> ConstructorHelper(int sz) {
      MyHashMap<Long, Value> m = new MyHashMap<Long, Value>(sz, map_counts++);
    System.out.println("\nCreated map number " + map_counts);
    return m;
  }

  public Bootstrap() {
    notInterestingMaps = new LinkedList<HashMap<Long, Value>>();
    map = ConstructorHelper(max);
    fillHashMapWithRandom(map);
    map1 = ConstructorHelper(max);
    fillHashMapWithRandom(map1);
    map2 = ConstructorHelper(max);
    fillHashMapWithRandom(map2);
    map3 = ConstructorHelper(max);
    fillHashMapWithRandom(map3);
    map4 = ConstructorHelper(max);
    fillHashMapWithRandom(map4);
    map5 = ConstructorHelper(max);
    fillHashMapWithRandom(map5);
    map6 = ConstructorHelper(max * 10);
    fillHashMapWithRandom(map6);
  }

  public MyHashMap<Long, Value> fillMapRandom() {
    fillHashMapWithRandom(map);
    return map;
  }

  public MyHashMap<Long, Value> fillMapValues() {
    fillHashMap(map1, map);
    return map1;
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

import java.util.HashMap;
import java.util.Random;
import java.util.Base64;

public class Bootstrap {

  // We'll have to try with Integer.MAX_VALUE, but with a bigger heap
  // to prevent OutOfMemory errors.
  public static int max = 25000;

  private HashMap<Long, String> map;
  private HashMap<Long, String> map1;
  private HashMap<Long, String> map2;
  private HashMap<Long, String> map3;
  private HashMap<Long, String> map4;
  private HashMap<Long, String> map5;
  private int map_counts = 0;

  public HashMap<Long, String> getMap0() { return map; }
  public HashMap<Long, String> getMap1() { return map1; }
  public HashMap<Long, String> getMap2() { return map2; }
  public HashMap<Long, String> getMap3() { return map3; }
  public HashMap<Long, String> getMap4() { return map4; }
  public HashMap<Long, String> getMap5() { return map5; }
  
  private HashMap<Long, String> ConstructorHelper(int sz) {
    HashMap<Long, String> m = new HashMap<Long, String>(sz);
    System.out.println("\nCreated map number " + ++map_counts);
    return m;
  }

  public Bootstrap() {
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
  }

  public HashMap<Long, String> fillMapRandom() {
    fillHashMapWithRandom(map);
    return map;
  }

  public HashMap<Long, String> fillMapValues() {
    fillHashMap(map1, map);
    return map1;
  }

  private void fillHashMapWithRandom(HashMap<Long, String> map) {
    if(map == null)
      return;

    Random generator = new Random((long)max);
    Base64.Encoder encoder = Base64.getEncoder();
    for(int i = 0; i < max; i++) {
      long key = generator.nextInt();
      byte[] stringBytes = new byte[6];
      generator.nextBytes(stringBytes);

      String value = encoder.encodeToString(stringBytes);

      map.put(key, value);
    }
  }

  private void fillHashMap(HashMap<Long, String> map1,
                           HashMap<Long, String> map2) {
    map1.putAll(map2);
    map1.put(27L, "Duarte");
    map1.put(42L, "Ivan");
    map1.put(27L, "Cândida"); // override
  }
}

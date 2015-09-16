/*
 * Fills a big hashmap with tons of entries, and then tries to compute
 * new hash values for it
 */
import java.util.Random;
import java.util.Base64;

public class BloatHashMapTest {

    // We'll have to try with Integer.MAX_VALUE, but with a bigger heap
    // to prevent OutOfMemory errors.
    private static int max = 100000;

    public static void main(String[] args) throws Exception {
      System.out.println("--------------- BloatHashMapTest STARTED ---------------");

      MyHashMap<Long, String> map = new MyHashMap<Long, String>(max);
      MyHashMap<Long, String> map1 = new MyHashMap<Long, String>(max / 2);
      MyHashMap<Long, String> map2 = new MyHashMap<Long, String>(max / 4);

      fillHashMapWithRandom(map);
      fillHashMapWithRandom(map1);
      fillHashMapWithRandom(map2);

      for(int i = 0; i < max / 2; i++) {
        MyHashMap<Long, String> young_map = new MyHashMap<Long, String>(i*2);
        fillHashMapWithRandom(young_map);
      }

      System.out.println("--------------- BloatHashMapTest DONE ---------------");
    }

  private static void fillHashMapWithRandom(MyHashMap<Long, String> map) {
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
}

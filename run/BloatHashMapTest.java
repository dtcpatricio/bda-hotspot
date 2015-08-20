/*
 * Fills a big hashmap with tons of entries, and then tries to compute
 * new hash values for it
 */
import java.util.HashMap;
import java.util.Random;
import java.util.Base64;

public class BloatHashMapTest {

    // We'll have to try with Integer.MAX_VALUE, but with a bigger heap
    // to prevent OutOfMemory errors.
    private static int max = 2000;

    public static void main(String[] args) throws Exception {
      System.out.println("--------------- BloatHashMapTest STARTED ---------------");

      HashMap<Long, String> map = new HashMap<Long, String>(max);
      HashMap<Long, String> map1 = new HashMap<Long, String>(max / 2);
      HashMap<Long, String> map2 = new HashMap<Long, String>(max / 4);

      fillHashMapWithRandom(map);
      fillHashMapWithRandom(map1);
      fillHashMapWithRandom(map2);

      for(int i = 0; i < max / 2; i++) {
        HashMap<Long, String> young_map = new HashMap<Long, String>(i*2);
        fillHashMapWithRandom(young_map);
      }

      System.out.println("--------------- BloatHashMapTest DONE ---------------");
    }

  private static void fillHashMapWithRandom(HashMap<Long, String> map) {
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
//            System.out.println("Added (" + key + " " + value + ") K/V pair");
        }
    }
}

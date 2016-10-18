import java.util.HashMap;
/*
 * Fills a big hashmap with tons of entries, and then tries to compute
 * new hash values for it
 */
public class BloatHashMapTest {

  public static int readCount = 1000000000;
  public static HashMap<Long, String> map;
  public static HashMap<Long, String> map1;

  public static void main(String[] args) throws Exception {
    System.out.println("--------------- BloatHashMapTest STARTED ---------------");

    Bootstrap bs  = new Bootstrap();
    int gc_count = 0;
    map = bs.fillMapRandom();
    map1 = bs.fillMapRandom();
    HashMap<Long, String> map2 = bs.getMap2();
    HashMap<Long, String> map3 = bs.getMap3();
    HashMap<Long, String> map4 = bs.getMap4();
    HashMap<Long, String> map5 = bs.getMap5();
    
    // Read values on and on and report time taken

    System.out.println("---- Starting to read ----");
    long startTime = System.nanoTime();

    int j;
    for (int i = 0; i < readCount; ++i) {
      j = i % bs.max;
      if (map.get(j) != null) {
        map.put((long)j, "Value scanned");
      }
    }
    for (int i = 0; i < readCount; ++i) {
      j = i % bs.max;
      if (map1.get(j) != null) {
        map1.put((long)j, "Value scanned");
      }
    }
    for (int i = 0; i < readCount; ++i) {
      j = i % bs.max;
      if (map2.get(j) != null) {
        map2.put((long)j, "Value scanned");
      }
    }
    for (int i = 0; i < readCount; ++i) {
      j = i % bs.max;
      if (map3.get(j) != null) {
        map3.put((long)j, "Value scanned");
      }
    }
    for (int i = 0; i < readCount; ++i) {
      j = i % bs.max;
      if (map4.get(j) != null) {
        map4.put((long)j, "Value scanned");
      }
    }
    for (int i = 0; i < readCount; ++i) {
      j = i % bs.max;
      if (map5.get(j) != null) {
        map5.put((long)j, "Value scanned");
      }
    }

    long elapsedTime = System.nanoTime() - startTime;
    double divisor = 1000000000.0;
    System.out.println("---- Finished reading in " + elapsedTime / divisor + " seconds. ----");
    

    System.out.println("--------------- BloatHashMapTest DONE ---------------");
  }


}

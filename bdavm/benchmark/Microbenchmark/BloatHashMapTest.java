/*
 * Fills a big hashmap with tons of entries, and then tries to compute
 * new hash values for it
 */
public class BloatHashMapTest {

    public static int readCount = 1000000000;
    public static MyHashMap<Long, Value> map;
    public static MyHashMap<Long, Value> map1;

    public static void main(String[] args) throws Exception {
        System.out.println("--------------- BloatHashMapTest STARTED ---------------");

        Bootstrap bs  = new Bootstrap();
        int gc_count = 0;
        map = bs.getInterestingMap(0);
        map1 = bs.getInterestingMap(1);
        MyHashMap<Long, Value> map2 = bs.getInterestingMap(2);
        MyHashMap<Long, Value> map3 = bs.getInterestingMap(3);
        MyHashMap<Long, Value> map4 = bs.getInterestingMap(4);
        MyHashMap<Long, Value> map5 = bs.getInterestingMap(5);
        MyHashMap<Long, Value> map6 = bs.getInterestingMap(6);
        
        // Read values on and on and report time taken

        System.out.println("---- Starting to read ----");
        long startTime = System.nanoTime();

        int j;
        for (int i = 0; i < readCount; ++i) {
            j = i % bs.max;
            if (map.get(j) != null) {
              Value v = map.get(j);
              Long  k = new Long(i);
              Value n = new Value(v.value() + "scanned", v.color());
              map.put(k, n);
            }
        }
        System.out.println("--- Read map with color " + map.color());
        
        for (int i = 0; i < readCount; ++i) {
            j = i % bs.max;
            if (map1.get(j) != null) {
                Value v = map1.get(j);
                Long  k = new Long(i * 1);
                Value n = new Value(v.value() + "scanned", v.color());
                map1.put(k, n);
            }
        }
        System.out.println("--- Read map with color " + map1.color());
        
        for (int i = 0; i < readCount; ++i) {
            j = i % bs.max;
            if (map2.get(j) != null) {
              Value v = map2.get(j);
              Long  k = new Long(i * 2);
              Value n = new Value(v.value() + "scanned", v.color());
              map2.put(k, n);
            }
        }
        System.out.println("--- Read map with color " + map2.color());
        
        for (int i = 0; i < readCount; ++i) {
            j = i % bs.max;
            if (map3.get(j) != null) {
              Value v = map3.get(j);
              Long  k = new Long(i * 3);
              Value n = new Value(v.value() + "scanned", v.color());
              map3.put(k, n);
            }
        }
        System.out.println("--- Read map with color " + map3.color());
        
        for (int i = 0; i < readCount; ++i) {
            j = i % bs.max;
            if (map4.get(j) != null) {
              Value v = map4.get(j);
              Long  k = new Long(i * 4);
              Value n = new Value(v.value() + "scanned", v.color());
              map4.put(k, n);
            }
        }
        System.out.println("--- Read map with color " + map4.color());
        
        for (int i = 0; i < readCount; ++i) {
            j = i % bs.max;
            if (map5.get(j) != null) {
              Value v = map5.get(j);
              Long  k = new Long(i * 5);
              Value n = new Value(v.value() + "scanned", v.color());
              map5.put(k, n);
            }
        }
        System.out.println("--- Read map with color " + map5.color());

        for (int i = 0; i < readCount / 2; ++i) {
          j = i % bs.max;
          if (map6.get(j) != null) {
            Value v = map6.get(j);
            Long  k = new Long(i * 6);
            Value n = new Value(v.value() + "scanned", v.color());
            map6.put(k, n);
          }
        }
        System.out.println("--- Read map with color " + map6.color());

        long elapsedTime = System.nanoTime() - startTime;
        double divisor = 1000000000.0;
        System.out.println("---- Finished reading in " + elapsedTime / divisor + " seconds. ----");
    

        System.out.println("--------------- BloatHashMapTest DONE ---------------");
    }


}

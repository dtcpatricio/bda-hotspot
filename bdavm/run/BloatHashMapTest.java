/*
 * Fills a big hashmap with tons of entries, and then tries to compute
 * new hash values for it
 */
public class BloatHashMapTest {

  public static MyHashMap<Long, String> map;
  public static MyHashMap<Long, String> map1;
  public static MyHashMap<Long, String> map2;
  // public static HashMap<Long, String> map3;
  // public static HashMap<Long, String> map4;

  public static void main(String[] args) throws Exception {
    System.out.println("--------------- BloatHashMapTest STARTED ---------------");

    Bootstrap bs  = new Bootstrap();
    int gc_count = 0;
    map = bs.fillMapRandom();
    map2 = bs.fillMapRandom();
    System.out.println("-- Invoking GC -- count = " + gc_count++);
    System.gc();
    map1 = bs.fillMapValues();
    System.out.println("-- Invoking GC -- count = " + gc_count++);
    System.gc();


    for (String s : map1.values()) {
      // Duarte overriden by Cândida
      if (s.equals("Ivan") | s.equals("Duarte") | s.equals("Cândida"))
        System.out.println(s);
    }

    System.out.println("--------------- BloatHashMapTest DONE ---------------");
  }


}

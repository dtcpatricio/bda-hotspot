/*
 * Fills a big hashmap with tons of entries, and then tries to compute
 * new hash values for it
 */
public class BloatHashMapTest {

    public static MyHashMap<Long, String> map;
    public static MyHashMap<Long, String> map1;

    public static void main(String[] args) throws Exception {
      System.out.println("--------------- BloatHashMapTest STARTED ---------------");

      Bootstrap bs = new Bootstrap();

      map = bs.fillMapRandom();
      map1 = bs.fillMapValues();

      // for (Long l : map.keySet()) {
      //     if(l > max * 10)
      //         System.out.print(l + "; ");
      // }

      for (String s : map1.values()) {
          // Duarte overriden by Cândida
          if (s.equals("Ivan") | s.equals("Duarte") | s.equals("Cândida"))
              System.out.println(s);
      }

      System.out.println("--------------- BloatHashMapTest DONE ---------------");
    }


}

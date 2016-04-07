import java.util.HashMap;

public class MyHashMap<K, V> extends HashMap<K, V> {

  public MyHashMap(int capacity) {
    super(capacity);
  }

  public void myput(K key, V value) {
    System.out.println("Invoking put");
    super.put(key,value);
    System.out.println("Finished invoking put");
  }
}

import java.util.HashMap;

public class MyHashMap<K, V> extends HashMap<K, V> {

  public MyHashMap(int capacity) {
    super(capacity);
  }

  public void myput(K key, V value) {
    super.put(key,value);
  }
}

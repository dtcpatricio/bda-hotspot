import java.util.HashMap;

public class MyHashMap<K, V> extends HashMap<K, V> {

  private int _map_color;
  private HashMap<Integer, Permission> permTable;
    
  public MyHashMap(int capacity, int color) {
    super(capacity);
    _map_color = color;
  }

  @Override
  public V put(K key, V value) {
    super.put(key,value);
    return value;
  }

  public int color() { return _map_color; }

  public void addPermTable(HashMap<Integer, Permission> m) { permTable = m; }

  public HashMap<Integer, Permission> getPermTable() { return permTable; }
}

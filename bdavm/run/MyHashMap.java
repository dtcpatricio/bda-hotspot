import java.util.HashMap;

public class MyHashMap<K, V> extends HashMap<K, V> {

    private int _map_color;
    
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
}

import java.util.HashMap;

public class KVStore<K, V> extends HashMap<K, V> {

    public KVStore ()
        {
            super();
        }
    
    public KVStore (int capacity)
        {
            super(capacity);
        }

    @Override
    public V put(K key, V value) {
        super.put(key,value);
        return value;
    }
}

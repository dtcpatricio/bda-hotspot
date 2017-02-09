import java.util.Random;
import java.util.Map;
import java.util.SortedMap;
import java.util.Set;
import java.util.HashMap;

import java.util.concurrent.ConcurrentSkipListMap;


public class FakeCassandra <K, CK, V>
{
    private final int recordCount;
    private final int columnCount;
    private final Random random;

    private Map<K, SortedMap<CK, V>> data;

    
    public FakeCassandra (int numberRecords, int numberFields)
    {
        this.recordCount = numberRecords;
        this.columnCount = numberFields;
        this.random = new Random (System.currentTimeMillis());
        this.data = new KVStore<K, SortedMap<CK, V>>();
    }

    public int getKeysCount()
    {
        if (data != null) {
            return data.size();
        } else {
            return 0;
        }
    }

    public int getFieldCount()
    {
        return columnCount;
    }

    public int getInitialRecordCount()
    {
        return recordCount;
    }

    /**
     * This kind of implements a standard interface to read and update values
     */
    public SortedMap<CK, V> getRecord(K key, SortedMap<CK, V> result)
    {
        if (data.containsKey(key)) {       
            return data.get(key);
        } else { return null; }
    }

    public HashMap<CK, V> read(K key, Set<CK> fields)
    {
        if (data.containsKey(key)) {
            SortedMap<CK, V> sm = data.get(key);
            HashMap<CK, V> result = new HashMap<CK, V>();
            for (Map.Entry<CK, V> e : sm.entrySet()) {
                if (fields.contains(e.getKey())) {
                    result.put(e.getKey(), e.getValue());
                }
            }
            return result;
        } else {
            return null;
        }
    }

    public int updateAll (K key, HashMap<CK, V> values)
    {
        if (data.containsKey(key)) {
            int ret = 0;
            SortedMap<CK, V> sm = data.get(key);
            for (Map.Entry<CK, V> e : values.entrySet()) {
                if (sm.containsKey(e.getKey())) {
                    sm.put(e.getKey(), e.getValue());
                    ret++;
                }
            }
            return ret;
        } else {
            return -1;
        }
    }

    public int insert (K key, SortedMap<CK, V> values)
    {
        if (!data.containsKey(key)) {
            ((KVStore<K, SortedMap<CK, V>>)data).put(key, values);
            return 0;
        } else {
            return -1;
        }
    }
}

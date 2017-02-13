import java.util.Random;
import java.util.HashMap;
import java.util.concurrent.ConcurrentSkipListSet;
import java.util.concurrent.ConcurrentSkipListMap;

import java.lang.IllegalAccessException;
import java.lang.NoSuchMethodException;
import java.lang.InstantiationException;
import java.lang.reflect.InvocationTargetException;

public class YCSBClone<K, CK, V>
{
  private final Random random;
  private final int operationCount;
  private final FakeCassandra<K, CK, V> db;
  private final Class<K>  classK;
  private final Class<CK> classCK;
  private final Class<V>  classV;

  public YCSBClone (FakeCassandra<K, CK, V> database, int operationcount,
                    Class<K> clsK, Class<CK> clsCK, Class<V> clsV)
                      
    {
      this.operationCount = operationcount;
      this.db = database;
      this.classK = clsK;
      this.classCK = clsCK;
      this.classV = clsV;
      this.random = new Random(891273791623L);
    }

  public void doWorkload (CassandraYCSBClone.READTYPE readtype,
                          int nThreads, float readratio) throws
    NoSuchMethodException,
    InstantiationException,
    IllegalAccessException,
    InvocationTargetException
    {
      int recordsCount = db.getKeysCount();
      int fieldsCount  = db.getFieldCount();
      int count = (int)(operationCount * readratio);
      int updatecount = operationCount - count;
      do {
        Integer keyVal;
        if (readtype == CassandraYCSBClone.READTYPE.RANDOM) {
          keyVal = new Integer(random.nextInt(recordsCount));
        } else {
          keyVal = new Integer(operationCount - count);
        }
        K key = classK.getConstructor(String.class).newInstance(keyVal.toString());
        ConcurrentSkipListSet<CK> columnKeys = new ConcurrentSkipListSet<CK>();
        int fieldCount = random.nextInt(fieldsCount);
        for (int i = 0; i < fieldsCount; i++) {
          Integer colKeyVal = new Integer(i);
          CK colKey =
            classCK.getConstructor(String.class).newInstance(colKeyVal.toString());
          columnKeys.add(colKey);
        }
        HashMap<CK, V> result = db.read(key, columnKeys);
      } while (count-- > 0);

      // If there are updates, do here
      while (updatecount-- > 0) {
        Integer keyVal;
        if (readtype == CassandraYCSBClone.READTYPE.RANDOM) {
          keyVal = new Integer(random.nextInt(recordsCount));
        } else {
          keyVal = new Integer(operationCount - count);
        }
        K key = classK.getConstructor(String.class).newInstance(keyVal.toString());
        HashMap<CK, V> columnUpdateMap = new HashMap<CK, V>();
        int fieldCount = random.nextInt(fieldsCount);
        for (int i = 0; i < fieldsCount; i++) {
          Integer colKeyVal = new Integer(i);
          Integer colValVal = new Integer(i << random.nextInt(8));
          CK colKey =
            classCK.getConstructor(String.class).newInstance(colKeyVal.toString());
          V  colVal = 
            classV.getConstructor(String.class).newInstance(colValVal.toString());
          columnUpdateMap.put(colKey, colVal);
        }
        int success = db.updateAll(key, columnUpdateMap);
      }
    }

  public void populateData() throws
    NoSuchMethodException,
    InstantiationException,
    IllegalAccessException,
    InvocationTargetException
    {
      try {
        int recordCount = db.getInitialRecordCount();
        int columnCount = db.getFieldCount();
        for (int i = 0; i < recordCount; i++) {
          Integer keyVal = new Integer (i); // help being ordered
          K key = classK.getConstructor(String.class).newInstance (keyVal.toString());
          ConcurrentSkipListMap<CK, V> m = new ConcurrentSkipListMap<CK, V>();
          for (int j = 0; j < columnCount; j++) {
            Integer colKeyVal = new Integer(j); // must be ordered
            Integer colValVal = new Integer(i^j + random.nextInt());
            CK colKey =
              classCK.getConstructor(String.class).newInstance (colKeyVal.toString());
            V  value  =
              classV.getConstructor(String.class).newInstance (colValVal.toString());
            m.put (colKey, value);
          }
          db.insert (key, m);
        }
      } catch (Exception e) {
        e.printStackTrace();
      }
    }
}

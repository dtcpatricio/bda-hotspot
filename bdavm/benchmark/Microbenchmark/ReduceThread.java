import java.lang.Runnable;
import java.lang.Math;

import java.util.Map;
import java.util.List;
import java.util.Iterator;
import java.util.LinkedList;

public class ReduceThread implements Runnable
{
  private MyHashMap<Long, List<Pair<Value, Value>>> sharedKeysMap;
  private MyHashMap<Long, Value>                    unsharedKeysMap;
  private int                                       id;
  
  private MyHashMap<Pair<Value, Value>, List<Long>> reducedSharedMap;
  private MyHashMap<Value, List<Long>>              reducedUnsharedMap;

  // Statistics stuff
  private long                             avgAccessTime;
  private int                                nElementsAccessed;
  private long                             avgWriteTime;
  private int                                nElementsWritten;

  public int                                       getThreadId           () { return id; }
  public MyHashMap<Pair<Value, Value>, List<Long>> getReducedSharedMap   () { return reducedSharedMap; }
  public MyHashMap<Value, List<Long>>              getReducedUnsharedMap () { return reducedUnsharedMap; }
  public long                                    getAvgAccessTime      () { return avgAccessTime;}
  public long                                    getAvgWriteTime       () { return avgWriteTime;}
  
  public ReduceThread (MyHashMap<Long, List<Pair<Value, Value>>> sharedMap,
                       MyHashMap<Long, Value> unsharedMap,
                       int threadId)
    {
      sharedKeysMap = sharedMap;
      unsharedKeysMap = unsharedMap;
      id = threadId;
      this.avgAccessTime = 0;
      this.avgWriteTime = 0;
      this.nElementsAccessed = 0;
      this.nElementsWritten = 0;
        
      reducedSharedMap = new MyHashMap<Pair<Value, Value>, List<Long>>(sharedMap.size(), sharedMap.color());
      reducedUnsharedMap = new MyHashMap<Value, List<Long>>(unsharedMap.size(), unsharedMap.color());
    }

  public void run ()
    {
      // What it does is find a value equal to another with a different key.
      // If it finds such, then it is added a xor'ed key and the value.
      // First reduce the shared keys
      Iterator itShared = sharedKeysMap.entrySet().iterator();
      while (itShared.hasNext()) {
        // Time and get block
        long start = System.nanoTime();
        Map.Entry<Long, List<Pair<Value, Value>>> entry =
          (Map.Entry<Long, List<Pair<Value, Value>>>)itShared.next();
        long elapsed = System.nanoTime() - start;
        calculateAvgAccessTime(elapsed);

        // Time and get block
        start = System.nanoTime();
        Iterator<Pair<Value, Value>> pairIt = entry.getValue().iterator();
        elapsed = System.nanoTime() - start;
        calculateAvgAccessTime(elapsed);
        
        while (pairIt.hasNext()) {
          // Time and get block
          start = System.nanoTime();
          Pair<Value, Value> pair = pairIt.next();
          elapsed = System.nanoTime() - start;
          calculateAvgAccessTime(elapsed);
          
          List<Long> v = reducePair (pair);

          // Time and write block
          start = System.nanoTime();
          reducedSharedMap.put(pair, v);
          elapsed = System.nanoTime() - start;
          calculateAvgWriteTime(elapsed);
          
          pairIt.remove();
        }
        itShared.remove();
      }
     
      // Second reduce the unshared keys. 
      Iterator it = unsharedKeysMap.entrySet().iterator();
      while (it.hasNext()) {
        // Time and get block
        long start = System.nanoTime();
        Map.Entry<Long, Value> entry = (Map.Entry<Long, Value>)it.next();
        long elapsed = System.nanoTime() - start;
        calculateAvgAccessTime(elapsed);
        
        it.remove();
        if (unsharedKeysMap.containsValue(entry.getValue())) {
          List<Long> v = reduceMatchingValues(entry);

          start = System.nanoTime();
          reducedUnsharedMap.put(entry.getValue(), v);
          elapsed = System.nanoTime() - start;
          calculateAvgWriteTime(elapsed);
        }
      }
    }

  private List<Long> reducePair (Pair<Value, Value> pair)
    {
      List <Long> list = new LinkedList<Long>();
      for (Map.Entry<Long, List<Pair<Value, Value>>> entry : sharedKeysMap.entrySet()) {
        // Time and get block
        long start = System.nanoTime();
        Iterator<Pair<Value, Value>> listIter = entry.getValue().iterator();
        long elapsed = System.nanoTime() - start;
        calculateAvgAccessTime(elapsed);
        
        while (listIter.hasNext()) {
          Pair<Value, Value> pairElem = listIter.next();
          if (pair.compareTo(pairElem) == 0 && !list.contains(entry.getKey())) {

            // Time and get block
            start = System.nanoTime();
            list.add(entry.getKey());
            elapsed = System.nanoTime() - start;
            calculateAvgAccessTime(elapsed);
            
            // If it's the same object, remove from the list
            // (the previous iterator will remove "pair")
            if (pair != pairElem) {
              listIter.remove();
            }
          }
        }
      }
      return list;
    }
  
  private List<Long> reduceMatchingValues (Map.Entry<Long, Value> entry)
    {
      List <Long> reducedList = new LinkedList<Long>();
      reducedList.add(entry.getKey());
      for (Map.Entry<Long, Value> e : unsharedKeysMap.entrySet()) {

        // Time and get block
        long start = System.nanoTime();
        if (e.getValue().value().equals(entry.getValue().value())) {
          long elapsed = System.nanoTime() - start;
          calculateAvgAccessTime(elapsed);
          
          reducedList.add(e.getKey());
        }
      }
      return reducedList;
    }
  
  private Value reduceListPairs (List<Pair<Value, Value>> list) throws Exception
    {
      // Reduce in two steps --- first the pairs then the reduction of each
      // pair with the next.
      byte[][] byteMatrix = new byte[list.size()][]; int i = 0;
      // First pass
      for (Pair<Value, Value> pair : list) {
        byte[] strBytes1 = pair.first().value().getBytes("UTF-8");
        byte[] strBytes2 = pair.second().value().getBytes("UTF-8");
        byte[] bytesRes  = xorByteArrays(strBytes1, strBytes2);
        byteMatrix[i] = bytesRes;
        i++;
      }
      // Second pass
      byte[] res = new byte[byteMatrix[0].length];
      for (byte[] arr : byteMatrix) {
        res = xorByteArrays(res, arr);
      }
      String newStr = new String (res);
      Value  newValue = new Value (newStr, sharedKeysMap.color());
      return newValue;
    }
  
  private byte[] xorByteArrays(byte[] a1, byte[] a2)
    {
      int sz        = Math.min(a1.length, a2.length);
      byte[] result = new byte[sz];
      for (int i = 0; i < sz; i++) {
        result[i] = (byte) (a1[i] ^ a2[i]);
      }
      return result;
    }

  private void calculateAvgAccessTime(long elapsed)
    {
      nElementsAccessed++;
      long delta = elapsed - avgAccessTime;
      avgAccessTime += delta / nElementsAccessed;
    }

  private void calculateAvgWriteTime(long elapsed)
    {
      nElementsWritten++;
      long delta = elapsed - avgWriteTime;
      avgWriteTime += delta / nElementsWritten;
    }
}

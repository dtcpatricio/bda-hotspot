import java.lang.Math;

import java.util.HashMap;
import java.util.Random;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.Iterator;
import java.util.LinkedHashSet;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

public class Bootstrap {

  // We'll have to try with Integer.MAX_VALUE, but with a bigger heap
  // to prevent OutOfMemory errors.
  private static int m_sizeMaps   = 0;
  private static int m_numberMaps = 0;
  private static int seed         = 31;
  private static int stringSz     = 8;

  private List<MyHashMap<Long, Value>>     dataMaps;
  private List<HashMap<Integer, Object[]>> systemMaps;
  private Logger                           m_logger;

  // also used as the color
  private int next_color = 0;

  // Static methods
  private static String byteArrayToHexString(byte[] b)
    {
      String result = "";
      for (int i = 0; i < b.length; i++) {
        result += Integer.toString ( (b[i] & 0xff ) + 0x100, 16).substring( 1 );
      }
      return result;
    }

  // Constructor and helpers
  public Bootstrap(int numberMaps, int sizeMaps, Logger logger)
    {
      m_numberMaps = numberMaps;
      m_sizeMaps = sizeMaps;
      m_logger = logger;
      dataMaps = new LinkedList<MyHashMap<Long, Value>>();
      systemMaps = new LinkedList<HashMap<Integer, Object[]>>();
    }
  
  public MyHashMap<Long, Value> getDataMapWithColor(int color)
    {
      if (color >= dataMaps.size())
        return null;
      else
        return dataMaps.get(color);
    }

  public int nDataMaps()
    {
      return dataMaps.size();
    }
  
  public void createUniverse() throws Exception
    {
      // create the random generator for large values
      Random generator = new Random ((long)m_sizeMaps * 1000);
      
      // create the "user's" maps (fictitious maps to fill space)

      // assume that the number of users is min(100, numberMaps / 2)
      int sUserMap = Math.min (100, (int) (m_numberMaps / 2.0));
      // the permissions map is = to sUserMaps in size
      int sPermMap = sUserMap;

      // create the user and permissions map.
      // Not init'ed to force the creation of garbage.
      HashMap<Integer, Object[]> userMap = new HashMap<Integer, Object[]>();
      // populate them
      for (int i = 0; i < sUserMap; i++) {
        int uid = generator.nextInt();
        byte[] nameBytes = new byte[stringSz];
        generator.nextBytes(nameBytes);
        String name = nameBytes.toString();
        String pass = generatePasswdHash(name.getBytes("UTF-8"));
        Object[] values = new Object[] { name, pass };
        userMap.put(uid, values);
        systemMaps.add(userMap);
      }

      // create the data maps and the pairing permissions table
      for (int i = 0; i < m_numberMaps; i++) {
        MyHashMap<Long, Value> data = new MyHashMap<Long, Value>(m_sizeMaps, next_color++);
        HashMap<Integer, Permission> permMap = new HashMap<Integer, Permission>();
        fillHashMapWithRandom (data, permMap, userMap);
        data.addPermTable(permMap);
        dataMaps.add(data);
      }
    }

  private String generatePasswdHash (byte[] bytes)
    {
      MessageDigest md = null;
      try {
        md = MessageDigest.getInstance("SHA-1");
      } catch (NoSuchAlgorithmException e) {
        e.printStackTrace();
      }
      return byteArrayToHexString (md.digest(bytes));
    }

  private void fillHashMapWithRandom(MyHashMap<Long, Value> map,
                                     HashMap<Integer, Permission> perm,
                                     HashMap<Integer, Object[]> user)
    {
      if(map == null)
        return;
    
      int color = map.color();
      Set userS = new LinkedHashSet(user.keySet());
      Iterator<Integer> userIt = userS.iterator();
      Random generator = new Random(seed);
      
      for(int i = 0; i < m_sizeMaps; i++) {
        long key = generator.nextLong();
        byte[] strBytes = new byte[stringSz];
        generator.nextBytes(strBytes);
        String str = strBytes.toString();
        Value value = new Value(str, color);

        // populate the permissions map
        if (i < userS.size() && userIt.hasNext()) {
          Integer kPerm = userIt.next();
          perm.put(kPerm, Permission.READWRITE);
        }
      
        map.put(key, value);
        m_logger.logInfo ("New value with key " + key + " and value " + value);
      }
    }
}

 

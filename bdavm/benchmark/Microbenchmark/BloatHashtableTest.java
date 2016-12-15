import java.util.Random;
import java.util.Base64;
import java.util.Hashtable;

public class BloatHashtableTest {

  private static long max_val = 1000000;
  public static void main(String[] args) {
    Hashtable<String, Long> t1 = new Hashtable<String, Long>();
    Hashtable<String, Long> t2 = new Hashtable<String, Long>();
    Hashtable<String, Long> t3 = new Hashtable<String, Long>();

    System.out.println("Fill hashtables test STARTED!");
    fillHashtable(t1);
    fillHashtable(t2);
    fillHashtable(t3);
    System.out.println("Fill hashtables FINISHED!");
  }

  public static void fillHashtable(Hashtable<String,Long> table) {
    Random generator = new Random(max_val);
    Base64.Encoder encoder = Base64.getEncoder();
    for(long i = 0; i < max_val; i++) {
      Long key = generator.nextLong();
      byte[] stringBytes = new byte[8];
      generator.nextBytes(stringBytes);

      String value = encoder.encodeToString(stringBytes);

      // inverted order
      table.put(value, key);
    }
  }
}

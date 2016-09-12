/*
 * Allocates dummy objects and outputs the number of objects allocated so far
 */
public class TestOptoAllocation {
  private static int count;
  private static int max = 3000;
  
  public static void main(String[] args) throws Exception {
    count = 0;

    for (int i = 0; i < max; i++) {
      Object o = allocate();
    }

    System.out.println("---------- TestOptoAllocation DONE ----------");
  }

  /* Alocates dummy objects */
  private static Object allocate() {
    Object o = new Object();
    System.out.println("Allocated object #" + ++count + " " + o);
    return o;
  }
}

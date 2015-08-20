import java.util.HashMap;

public class SimpleMainTest {
    private static HashMap<Integer,String> map;

    public static void main(String[] args) {
        map = new HashMap<Integer, String>(40);
        System.out.println("Putting new value in HashMap");
        map.put(1, "Alfredo");
        System.out.println("Getting value " + map.get(1));
    }
}

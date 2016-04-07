public class SimpleMainTest {
    private static MyHashMap<Integer,String> map;

    public static void main(String[] args) {
        map = new MyHashMap<Integer, String>(100);
        System.out.println("Putting new value in HashMap");
        map.myput(1, "Alfredo");
        System.out.println("Getting value " + map.get(1));
    }
}

import java.util.Random;
import java.util.Base64;

public class Bootstrap {

    // We'll have to try with Integer.MAX_VALUE, but with a bigger heap
    // to prevent OutOfMemory errors.
    private static int max = 1000000;

    private MyHashMap<Long, String> map;
    private MyHashMap<Long, String> map1;
    private MyHashMap<Long, String> map2;
    private MyHashMap<Long, String> map3;
    private MyHashMap<Long, String> map4;
    private MyHashMap<Long, String> map5;
    private int map_counts = 0;

    private MyHashMap<Long, String> ConstructorHelper(int sz) {
        MyHashMap<Long, String> m = new MyHashMap<Long, String>(sz);
        System.out.println("\nCreated map number " + ++map_counts);
        return m;
    }

    public Bootstrap() {
        map = ConstructorHelper(max);
        fillHashMapWithRandom(map);
        map1 = ConstructorHelper(max * 2);
        fillHashMapWithRandom(map1);
        map2 = ConstructorHelper(max * 7);
        fillHashMapWithRandom(map2);
        map3 = ConstructorHelper(max * 6);
        fillHashMapWithRandom(map3);
        map4 = ConstructorHelper(max * 3);
        fillHashMapWithRandom(map4);
        map5 = ConstructorHelper(max * 4);
        fillHashMapWithRandom(map5);
    }

    public MyHashMap<Long, String> fillMapRandom() {
        fillHashMapWithRandom(map);
        return map;
    }

    public MyHashMap<Long, String> fillMapValues() {
        fillHashMap(map1, map);
        return map1;
    }

    private void fillHashMapWithRandom(MyHashMap<Long, String> map) {
        if(map == null)
            return;

        Random generator = new Random((long)max);
        Base64.Encoder encoder = Base64.getEncoder();
        for(int i = 0; i < max; i++) {
            long key = generator.nextInt();
            byte[] stringBytes = new byte[6];
            generator.nextBytes(stringBytes);

            String value = encoder.encodeToString(stringBytes);

            map.myput(key, value);

            if(i < 50)
                System.out.print(" " + map.get(key) + " ");
        }
    }

    private void fillHashMap(MyHashMap<Long, String> map1,
                             MyHashMap<Long, String> map2) {
        map1.putAll(map2);
        map1.put(27L, "Duarte");
        map1.put(42L, "Ivan");
        map1.put(27L, "Cândida"); // override
    }
}

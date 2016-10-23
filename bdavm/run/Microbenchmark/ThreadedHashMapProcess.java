import java.util.List;
import java.util.ArrayList;
import java.util.LinkedList;
import java.lang.Thread;

public class ThreadedHashMapProcess {

    public static int readCount = 100000000;
    public static int thrdCount = 8;

    public static void main(String[] args) throws Exception {
        System.out.println("--------------- Threaded BloatHashMapTest STARTED ---------------");

        Bootstrap bs  = new Bootstrap();
        List<Thread> threadPool = new ArrayList<Thread>(thrdCount);
        
        int numberOfMaps = bs.numberOfInterestingMaps();
        int numberOfMapsPerThread = (int) Math.round(numberOfMaps / (float)thrdCount);
        System.out.println("Each thread shall run " + numberOfMapsPerThread + " maps.");
        for (int i = 0; i < thrdCount; ++i) {
            List<MyHashMap<Long, Value>> mapsList =
                new ArrayList<MyHashMap<Long, Value>>(numberOfMapsPerThread);
            for(int j = 0; j < numberOfMapsPerThread; ++j) {
                MyHashMap<Long, Value> m = bs.getInterestingMap(i * numberOfMapsPerThread + j);
                if (m != null)
                    mapsList.add(m);
                else
                    System.out.println("Overflowing with maps");
            }
            ProcessThread thr = new ProcessThread(mapsList, i, readCount);
            threadPool.add(thr);
        }
        // Read values on and on and report time taken

        System.out.println("---- Starting to read ----");
        long startTime = System.nanoTime();

        for (int i = 0; i < thrdCount; ++i) {
            threadPool.get(i).start();
        }

        for (int i = 0; i < thrdCount; ++i) {
            threadPool.get(i).join();
        }

        long elapsedTime = System.nanoTime() - startTime;
        double divisor = 1000000000.0;
        System.out.println("---- Finished reading in " + elapsedTime / divisor + " seconds. ----");
    

        System.out.println("--------------- BloatHashMapTest DONE ---------------");
    }


}

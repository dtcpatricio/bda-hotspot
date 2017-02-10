import java.util.List;
import java.util.ArrayList;
import java.util.LinkedList;
import java.lang.Thread;

public class ThreadedHashMapProcess {

    public static void main(String[] args) throws Exception {

        int readCount = 1;
        int thrdCount = 1;
        int mapCount = 1;
        int mapSize = 1;
        int idx = -1;
        
        while (++idx < args.length) {
            try {
                if (args[idx].equals("-threads")) {
                    thrdCount = Integer.parseInt(args[++idx]);
                } else if (args[idx].equals("-p")) {
                    if (args[++idx].startsWith("readcount")) {
                        readCount = Integer.parseInt(args[idx].split("=")[1]);
                    } else if (args[idx].startsWith("mapcount")) {
                        mapCount = Integer.parseInt(args[idx].split("=")[1]);
                    } else if (args[idx].startsWith("mapsize")) {
                        mapSize = Integer.parseInt(args[idx].split("=")[1]);
                    } else {
                        printHelpExit(0);
                    }
                } else {
                    printHelpExit(0);
                }
            } catch (Exception e) {
                errorExitStartup();
            }
        }
        
        System.out.println("# --------------- Threaded BloatHashMapTest STARTED ---------------");
        System.out.println("# Threads: " + thrdCount);
        System.out.println("# Maps: " + mapCount);
        System.out.println("# Map Size: " + mapSize);
        System.out.println("# Reads: " + readCount);
        
        Logger bsLog = new Logger();
        Bootstrap bs = new Bootstrap(mapCount, mapSize, bsLog);
        bs.createUniverse();
        List<Thread> threadPool = new ArrayList<Thread>(thrdCount);
        
        int numberOfMapsPerThread = (int) Math.round(mapCount / (float)thrdCount);
        System.out.println("Each thread shall run " + numberOfMapsPerThread + " maps.");
        for (int i = 0; i < thrdCount; ++i) {
            List<MyHashMap<Long, Value>> mapsList =
                new ArrayList<MyHashMap<Long, Value>>(numberOfMapsPerThread);
            for(int j = 0; j < numberOfMapsPerThread; ++j) {
                MyHashMap<Long, Value> m = bs.getDataMapWithColor(i * numberOfMapsPerThread + j);
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


    private static void errorExitStartup()
    {
        System.err.println("Wrong number or value of arguments.");
        printHelpExit(-1);
    }
    
    private static void printHelpExit(int code)
    {
        System.err.println("Usage:");
        System.err.println("  ThreadedHashMapProcess [options]");
        System.err.println("    [options] can be:");
        System.err.println("      -p");
        System.err.println("        readcount=<value>\t Number of reads to perform");
        System.err.println("        mapcount=<value>\t Number of maps to create");
        System.err.println("        mapsize=<value>\t Size, in entries, of the maps added");
        System.err.println("      -threads <num>\t Number of threads to use for processing");
        System.exit (code);
    }
}

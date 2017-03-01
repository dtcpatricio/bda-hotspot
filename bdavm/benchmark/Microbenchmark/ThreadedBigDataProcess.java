import java.util.List;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.Hashtable;
import java.util.Map;

import java.lang.Thread;


public class ThreadedBigDataProcess {

  private static double secDivisor = 1000000000.0;
  public static int readCount = 100000000;
  

  private enum RUNTIME
    {
      UNKNOWN,
      SHARED,
      INDEPENDENT
    }

  private enum PHASE
    {
      MAP,
      REDUCE
    }
  
  private static void launchPhase (Hashtable<Thread, Runnable> threadPool, PHASE phase) throws Exception
    {
      System.out.println("---- Starting " + phase.name() + " phase ----");
      long startTime = System.nanoTime();
      for (Map.Entry<Thread, Runnable> e : threadPool.entrySet()) {
        e.getKey().start();
      }
      for (Map.Entry<Thread, Runnable> e : threadPool.entrySet()) {
        e.getKey().join();
      }
      long elapsedTime = System.nanoTime() - startTime;
      System.out.println("---- Finished " + phase.name() + " phase in " + elapsedTime / secDivisor +
                         " seconds. ----");
    }
  
  private static void errorExitStartup()
    {
      System.err.println("Wrong number or value of arguments.");
      listHelpExit();
    }

  private static void listHelpExit()
    {
      System.out.println("Usage:");
      System.out.println("ThreadedBigDataProcess <shared/independent> <thread num> <number of maps> <size of maps>");
      System.exit(1);
    }

  private static RUNTIME parseRuntimeOption (String opt)
    {
      if (opt.equals("shared")) {
        return RUNTIME.SHARED;
      } else if (opt.equals("independent")) {
        return RUNTIME.INDEPENDENT;
      } else {
        errorExitStartup();
        return RUNTIME.UNKNOWN;
      }
    }

  private static void printUnsharedMap (Map<Value, List<Long>> map)
    {
      for (Map.Entry<Value, List<Long>> e : map.entrySet()) {
        System.out.println ("    Key " + e.getKey() + " Value " + e.getValue());
      }
    }

  private static void printSharedMap (Map<Pair<Value, Value>, List<Long>> map)
    {
      for (Map.Entry<Pair<Value, Value>, List<Long>> e : map.entrySet()) {
        System.out.println ("    Key " + e.getKey() + " Value " + e.getValue());
      }
    }

  
  public static void main(String[] args) throws Exception {

    int nThreads      = 0;
    int sMaps         = 0;
    int nMaps         = 0;
    boolean printMaps = false;
    RUNTIME type      = RUNTIME.UNKNOWN;
      
    if (args.length < 4) {
      if (args.length == 0 || !args[0].equals("-h") || !args[0].equals("--help")) {
        errorExitStartup();
      } else {
        listHelpExit();
      }
    } else {
      type     = parseRuntimeOption(args[0]);
      nThreads = Integer.parseInt(args[1]);
      nMaps    = Integer.parseInt(args[2]);
      sMaps    = Integer.parseInt(args[3]);
      if (args.length > 4 && args[4].equals("-print")) {
        printMaps = true;
      }
    }
    

    System.out.println("--------------- Threaded BigData Process STARTED ---------------");

    Logger bsLog = new Logger();
    Bootstrap bs = new Bootstrap(nMaps, sMaps, bsLog);

    bs.createUniverse();

    // Do scheduling for map phase
    Hashtable<Thread, Runnable> mapThreadPool = new Hashtable<Thread, Runnable>(nThreads);
    int numberOfMaps = bs.nDataMaps();
    int numberOfMapsPerThread = (int) Math.round(numberOfMaps / (float)nThreads);
    int sharedColor = ( (nMaps & 0x1) == 0 ) ? nMaps : nMaps + 1; // give it an even index
    int unsharedColor = sharedColor + 1;
    System.out.println("Each thread shall run " + numberOfMapsPerThread + " maps.");
    for (int i = 0; i < nThreads; ++i) {
      List<MyHashMap<Long, Value>> mapsList =
        new ArrayList<MyHashMap<Long, Value>>(numberOfMapsPerThread);
      for(int j = 0; j < numberOfMapsPerThread; ++j) {
        MyHashMap<Long, Value> m = bs.getDataMapWithColor(i * numberOfMapsPerThread + j);
        if (m != null)
          mapsList.add(m);
        else
          System.out.println("Overflowing with maps");
      }
      Runnable run = new MapThread(mapsList, i, sMaps, sharedColor++, unsharedColor++);
      Thread thr = new Thread(run);
      mapThreadPool.put(thr, run);
    }
    // Launch map phase
    launchPhase (mapThreadPool, PHASE.MAP);
    // Get access time and write time
    System.out.println("# Printing thread access and write/update time:");
    for (Map.Entry<Thread, Runnable> e : mapThreadPool.entrySet()) {
      MapThread thr = (MapThread)e.getValue();
      System.out.println("Thread " + thr.getThreadId() + " access time " +
                         (thr.getAvgAccessTime()/secDivisor) + " s -- write/update time " +
                         (thr.getAvgWriteTime()/secDivisor) + " s");
    }


    // Schedule reduce phase getting the mapped data from the previous step.
    Hashtable<Thread, Runnable> reduceThreadPool = new Hashtable<Thread, Runnable>(nThreads);
    for (Map.Entry<Thread, Runnable> e : mapThreadPool.entrySet()) {
      Runnable run = new ReduceThread(((MapThread)e.getValue()).getReducedSharedMap(),
                                      ((MapThread)e.getValue()).getReducedUnsharedMap(),
                                      ((MapThread)e.getValue()).getThreadId());
      Thread thr = new Thread (run);
      reduceThreadPool.put(thr, run);
    }
    launchPhase (reduceThreadPool, PHASE.REDUCE);
    // Get access time and write time
    System.out.println("# Printing thread access and write/update time:");
    for (Map.Entry<Thread, Runnable> e : reduceThreadPool.entrySet()) {
      ReduceThread thr = (ReduceThread)e.getValue();
      System.out.println("Thread " + thr.getThreadId() + " access time " +
                         ((double)thr.getAvgAccessTime() / secDivisor) + " s -- write/update time " +
                         ((double)thr.getAvgWriteTime() / secDivisor) + " s");
    }

    // Print results
    if (printMaps) {
      for (Map.Entry<Thread, Runnable> e : reduceThreadPool.entrySet()) {
        System.out.println ("Thread " + ((ReduceThread)e.getValue()).getThreadId());
        System.out.println ("  Reduced Shared Map:");
        printSharedMap (((ReduceThread)e.getValue()).getReducedSharedMap());
        System.out.println ("  Reduced Unshared Map:");
        printUnsharedMap (((ReduceThread)e.getValue()).getReducedUnsharedMap());
        System.out.println ();
      }
    }
    
    System.out.println("--------------- Threaded BigData Process DONE ---------------");
  }
}

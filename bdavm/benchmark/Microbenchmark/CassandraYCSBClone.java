
public class CassandraYCSBClone
{
    private static double secDivior = 1000000000.0;

    public static enum READTYPE
        {
            SEQUENTIAL, RANDOM
        }
    
    public static void main (String[] args) throws Exception
        {
            int recordcount    = 1;
            int operationcount = 1;
            int nThreads       = 1;
            int numberfields   = 1;
            float readratio    = 1.0f;
            READTYPE readtype  = READTYPE.SEQUENTIAL;
        
            int idx = -1;
            while (++idx < args.length) {
                try {
                    if (args[idx].equals("-p")) {
                        if (args[++idx].startsWith("recordcount")) {
                            recordcount = Integer.parseInt(args[idx].split("=")[1]);
                        } else if (args[idx].startsWith("operationcount")) {
                            operationcount = Integer.parseInt(args[idx].split("=")[1]);
                        } else if (args[idx].startsWith("numberfields")) {
                            numberfields = Integer.parseInt(args[idx].split("=")[1]);
                        }
                    } else if (args[idx].equals("-t")) {
                        if (args[++idx].equals("random")) {
                            readtype = READTYPE.RANDOM;
                        }
                    } else if (args[idx].equals("-threads")) {
                        nThreads = Integer.parseInt(args[++idx]);
                    } else if (args[idx].equals("-readratio")) {
                      readratio = Float.parseFloat(args[++idx]);
                    } else if (args[idx].equals("-h") || args[idx].equals("--help")) {
                        printHelpExit(0);
                    } else {
                        printHelpExit(0);
                    }
                } catch (Exception e) {
                    printHelpExit(-1);
                }
            }

            if (recordcount <= 0 || operationcount <= 0 ||
                readratio > 1.0 || readratio < 0.0) {
                System.out.println ("Error: No operationcount or recordcount");
                printHelpExit(-1);
            }

            System.out.println("# YCSBClone and FakeCassandra test");
            System.out.println("# Record Count: " + recordcount);
            System.out.println("# Operation Count: " + operationcount);
            System.out.println("#   Reads: " + (int)(operationcount * readratio));
            System.out.println("#   Updates: " + (operationcount - (int)(operationcount * readratio)));
            System.out.println("# Field Count: " + numberfields);
            System.out.println("# Threads: " + nThreads);

            System.out.println("-- Loading FakeCassandra --");
            // Create the "Cassandra" Lite instance, filling it with data
            FakeCassandra<Long, Long, String> fc =
                new FakeCassandra<Long, Long, String> (recordcount, numberfields);

            System.out.println("-- Quering with YCSBClone --");
            // Start the YCSBClone and begin query/update the FakeCassandra KVS
            YCSBClone<Long, Long, String> ycsb =
                new YCSBClone<Long, Long, String>(fc, operationcount,
                                                  Long.class, Long.class, String.class);
            ycsb.populateData();
            ycsb.doWorkload(readtype, nThreads, readratio);
        }

    private static void errorExitStartup()
        {
            System.err.println("Wrong number or value of arguments.");
            printHelpExit(-1);
        }

    private static void printHelpExit(int code)
        {
            System.err.println("Usage:");
            System.err.println("  CassandraYCSBClone [options]");
            System.err.println("    [options] can be:");
            System.err.println("      -p");
            System.err.println("        recordcount=<value>\t Number of records to fill the KVS");
            System.err.println("        operationcount=<value>\t Number of operations to perform");
            System.err.println("        numberfields=<value>\t Number of columns per record");
            System.err.println("      -threads <num>\t Number of threads to use for processing");
            System.exit (code);
        }
}

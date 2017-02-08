
public class CassandraYCSBClone
{
    private static double secDivior = 1000000000.0;

    private static enum READTYPE
    {
        SEQUENTIAL, RANDOM
    }
    
    public static void main (String[] args) throws Exception
    {
        int recordcount    = 0;
        int operationcount = 0;
        int nThreads       = 1;
        
        int idx = -1;
        while (++idx < args.length) {
            try {
                if (args[idx++].equals("-p")) {
                    if (args[idx].startsWith("recordcount")) {
                        recordcount = Integer.parseInt(args[idx].split("=")[1]);
                    } else if (args[idx].startsWith("operationcount")) {
                        operationcount = Integer.parseInt(args[idx].split("=")[1]);
                    }
                } else if (args[idx++].equals("-threads")) {
                    nThreads = Integer.parseInt(args[idx]);
                }
            } catch (Exception e) {
                printHelpExit();
            }
        }

        if (recordcount <= 0 || operationcount <= 0) {
            System.out.println ("Error: No operationcount or recordcount");
            printHelpExit();
        }

        // Create the "Cassandra" Lite instance, filling it with data
        FakeCassandra fc = new FakeCassandra (recordcount);

        
        
    }
}

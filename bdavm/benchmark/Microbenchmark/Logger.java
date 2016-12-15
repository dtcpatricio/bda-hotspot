import java.util.Date;
import java.util.HashMap;

public class Logger {

  // All goes to the same map
  private HashMap<Long, Value> log;
  
  public Logger ()
    {
      Date now = new Date();
      log = new HashMap<Long, Value>();
      log.put(now.getTime(), new Value("Init", -1));
    }

  public void logInfo (String msg)
    {
      Date now = new Date();
      log.put(now.getTime(), new Value("(INFO) " + msg, -1));
    }
}

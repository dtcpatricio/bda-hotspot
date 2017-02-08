
public class FakeCassandra
{
    private int recordCount = 0;

    private Map<Key, SortedMap<Key, Value>> data;
    
    public FakeCassandra (int records)
        {
            this.recordCount = records;
            
        }
}

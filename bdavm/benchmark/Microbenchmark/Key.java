public class Key<T>
{
    private T keyId;

    public Key (T id)
        {
            this.keyId = id;
        }

    public T key () { return keyId; }
}

import java.lang.Comparable;

public class Pair<X, Y> implements Comparable<Pair<X, Y>>
{
  private final X first;
  private final Y second;

  public Pair(X x, Y y)
    {
      super();
      first = x;
      second = y;
    }

  public X first()  { return first; }
  public Y second() { return second; }
  
  @Override
  public String toString()
    {
      return "( " + first + ", " + second + " )";
    }

  public int compareTo(Pair<X, Y> p)
    {
      if ((this.first().equals(p.first()) && this.second().equals(p.second())) ||
          (this.first().equals(p.second()) && this.second().equals(p.first()))) {
        return 0;
      } else {
        return -1;
      }
    }
}

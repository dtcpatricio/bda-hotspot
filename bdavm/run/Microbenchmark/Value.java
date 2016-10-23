public class Value {

    private String _value;
    private int    bdaColor;

    Value (String value, int color) {
        _value = value;
        bdaColor = color;
    }

    public String value() { return _value; }
    public int    color() { return bdaColor; }

    public Value  set_value(String v) {
        _value = v;
        return this;
    }
}

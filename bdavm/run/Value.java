public class Value {

    private String _value;
    private int    _color;

    Value (String value, int color) {
        _value = value;
        _color = color;
    }

    public String value() { return _value; }
    public int    color() { return _color; }

    public Value  set_value(String v) {
        _value = v;
        return this;
    }
}

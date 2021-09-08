package java.lang;



public final class Byte extends Number implements Comparable<Byte> {


    private byte value;


    public Byte(byte b)
    {
        value = b;
    }


    public static Byte valueOf(byte b)
    {
        return new Byte(b);
    }


    public static Byte valueOf(String s, int radix) throws NumberFormatException
    {
        return valueOf(parseByte(s, radix));
    }


    public static Byte valueOf(String s) throws NumberFormatException
    {
        return valueOf(s, 10);
    }


    public static String toString(byte b)
    {
        return Integer.toString((int)b, 10);
    }


    public String toString()
    {
        return toString(value);
    }


    public static byte parseByte(String s, int radix)
        throws NumberFormatException
    {
        int i = Integer.parseInt(s, radix);
        if (i < -128 || i > 128) {
            throw new NumberFormatException("value does not fit in byte");
        }
        return (byte)i;
    }


    public int compareTo(Byte other)
    {
        return compare(this.value, other.value);
    }


    public static int compare(byte x, byte y)
    {
        return x - y;
    }


    public byte byteValue()
    {
        return value;
    }


    public short shortValue()
    {
        return (short)value;
    }


    public int intValue()
    {
        return (int)value;
    }


    public long longValue()
    {
        return (long)value;
    }


    public float floatValue()
    {
        return (float)value;
    }


    public double doubleValue()
    {
        return (double)value;
    }


}

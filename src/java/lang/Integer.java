package java.lang;



class Integer extends Number implements Comparable<Integer> {


    private int value;


    Integer(int value)
    {
        this.value = value;
    }


    public int compareTo(Integer other)
    {
        return (this.value < other.value) ? -1 : ((this.value == other.value) ? 0 : 1);
    }


    public static Integer valueOf(int i)
    {
        return new Integer(i);
    }


    public byte byteValue()
    {
        return (byte)value;
    }


    public short shortValue()
    {
        return (short)value;
    }


    public int intValue()
    {
        return value;
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


    public static String toString(int i, int radix)
    {
        return IntegerStringConverter.toString(i, radix);
    }


    public static String toString(int i)
    {
        return IntegerStringConverter.toString(i);
    }


    public String toString()
    {
        return toString(value);
    }

}

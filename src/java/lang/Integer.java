package java.lang;



class Integer extends Number implements Comparable<Integer> {


    private final int value;


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


    public static int parseInt(String s) throws NumberFormatException
    {
        return parseInt(s,10);
    }


    public static int parseInt(String s, int radix)
        throws NumberFormatException
    {
        if (s == null) {
            throw new NumberFormatException("null");
        }

        if (radix < 2) {
            throw new NumberFormatException("radix " + radix +
                                            " less than Character.MIN_RADIX");
        }

        if (radix > 36) {
            throw new NumberFormatException("radix " + radix +
                                            " greater than Character.MAX_RADIX");
        }

        int result = 0;
        boolean negative = false;
        int i = 0, len = s.length();
        int limit = -0x7fffffff;
        int multmin;
        int digit;

        if (len > 0) {
            char firstChar = s.charAt(0);
            if (firstChar < '0') {
                if (firstChar == '-') {
                    negative = true;
                    limit = 0x80000000;
                } else if (firstChar != '+') {
                    throw new NumberFormatException(s);
                }

                if (len == 1) {
                    throw new NumberFormatException(s);
                }
                i++;
            }
            multmin = limit / radix;
            while (i < len) {
                digit = Character.digit(s.charAt(i++), radix);
                if (digit < 0) {
                    throw new NumberFormatException(s);
                }
                if (result < multmin) {
                    throw new NumberFormatException(s);
                }
                result *= radix;
                if (result < limit + digit) {
                    throw new NumberFormatException(s);
                }
                result -= digit;
            }
        } else {
            throw new NumberFormatException(s);
        }
        return negative ? result : -result;
    }

}

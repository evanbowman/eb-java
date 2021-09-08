package java.lang;



class Long extends Number implements Comparable<Long> {


    private final long value;


    public Long(long value)
    {
        this.value = value;
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
        return (int)value;
    }


    public long longValue()
    {
        return value;
    }


    public float floatValue()
    {
        return (float)value;
    }


    public double doubleValue()
    {
        return (double)value;
    }


    public int compareTo(Long other)
    {
        return compare(this.value, other.value);
    }


    public static int compare(long x, long y)
    {
        return (x < y) ? -1 : ((x == y) ? 0 : 1);
    }


    public static String toString(long l)
    {
        return IntegerStringConverter.toString(l);
    }


    public String toString()
    {
        return toString(value);
    }


    public static long parseLong(String s, int radix)
        throws NumberFormatException
    {
        if (s == null) {
            throw new NumberFormatException("null");
        }

        if (radix < 2) {
            throw new NumberFormatException("radix less than min radix");
        }
        if (radix > 36) {
            throw new NumberFormatException("radix greater than max radix");
        }

        long result = 0;
        boolean negative = false;
        int i = 0, len = s.length();
        long limit = -0x7fffffffffffffffL;
        long multmin;
        int digit;

        if (len > 0) {
            char firstChar = s.charAt(0);
            if (firstChar < '0') { // Possible leading "+" or "-"
                if (firstChar == '-') {
                    negative = true;
                    limit = 0x8000000000000000L;
                } else if (firstChar != '+')
                    throw new NumberFormatException(s);

                if (len == 1) // Cannot have lone "+" or "-"
                    throw new NumberFormatException(s);
                i++;
            }
            multmin = limit / radix;
            while (i < len) {
                // Accumulating negatively avoids surprises near MAX_VALUE
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

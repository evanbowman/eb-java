package java.lang;



public final class String implements Comparable<String>, CharSequence {


    private final char[] value;


    public String()
    {
        value = new char[0];
    }


    public String(String original)
    {
        this.value = original.value;
    }


    public String(char[] value)
    {
        this.value = value.clone();
    }


    String(char[] value, boolean share)
    {
        this.value = value;
    }


    public boolean isEmpty()
    {
        return value.length == 0;
    }


    public char charAt(int index)
    {
        if ((index < 0) || (index >= value.length)) {
            throw new StringIndexOutOfBoundsException(index);
        }
        return value[index];
    }


    public char[] toCharArray()
    {
        return value.clone();
    }


    public int length()
    {
        return value.length;
    }


    public static String valueOf(Object obj)
    {
        return (obj == null) ? "null" : obj.toString();
    }


    public static String valueOf(char[] data)
    {
        return new String(data);
    }


    public static String valueOf(char data[], int offset, int count)
    {
        return new String(data, offset, count);
    }


    public static String valueOf(byte b)
    {
        return Byte.toString(b);
    }


    public static String valueOf(boolean b)
    {
        return Boolean.toString(b);
    }


    public static String valueOf(char c)
    {
        return Character.toString(c);
    }


    public static String valueOf(int i)
    {
        return Integer.toString(i);
    }


    public static String valueOf(long l)
    {
        return Long.toString(l);
    }


    // public static String valueOf(float f)
    // {
    //     return Float.toString(f);
    // }


    public boolean equals(Object other)
    {
        if (this == other) {
            return true;
        }

        if (other instanceof String) {
            String anotherString = (String)other;
            int n = value.length;

            if (n == anotherString.value.length) {
                char v1[] = value;
                char v2[] = anotherString.value;
                int i = 0;

                while (n-- != 0) {
                    if (v1[i] != v2[i])
                        return false;
                    i++;
                }
                return true;
            }
        }

        return false;
    }


    public boolean equalsIgnoreCase(String anotherString)
    {
        return (this == anotherString) ? true
                : (anotherString != null)
                && (anotherString.value.length == value.length)
                && regionMatches(true, 0, anotherString, 0, value.length);
    }


    public boolean regionMatches(int toffset,
                                 String other,
                                 int ooffset,
                                 int len)
    {
        char ta[] = value;
        int to = toffset;
        char pa[] = other.value;
        int po = ooffset;
        if ((ooffset < 0) || (toffset < 0)
                || (toffset > (long)value.length - len)
                || (ooffset > (long)other.value.length - len)) {
            return false;
        }
        while (len-- > 0) {
            if (ta[to++] != pa[po++]) {
                return false;
            }
        }
        return true;
    }


    public boolean regionMatches(boolean ignoreCase,
                                 int toffset,
                                 String other,
                                 int ooffset,
                                 int len)
    {
        char ta[] = value;
        int to = toffset;
        char pa[] = other.value;
        int po = ooffset;
        if ((ooffset < 0) || (toffset < 0)
                || (toffset > (long)value.length - len)
                || (ooffset > (long)other.value.length - len)) {
            return false;
        }
        while (len-- > 0) {
            char c1 = ta[to++];
            char c2 = pa[po++];
            if (c1 == c2) {
                continue;
            }
            if (ignoreCase) {
                char u1 = Character.toUpperCase(c1);
                char u2 = Character.toUpperCase(c2);
                if (u1 == u2) {
                    continue;
                }
                if (Character.toLowerCase(u1) == Character.toLowerCase(u2)) {
                    continue;
                }
            }
            return false;
        }
        return true;
    }


    public int compareTo(String anotherString)
    {
        int len1 = value.length;
        int len2 = anotherString.value.length;
        int lim = Math.min(len1, len2);
        char v1[] = value;
        char v2[] = anotherString.value;

        int k = 0;
        while (k < lim) {
            char c1 = v1[k];
            char c2 = v2[k];
            if (c1 != c2) {
                return c1 - c2;
            }
            k++;
        }
        return len1 - len2;
    }


    public String toString()
    {
        return this;
    }


    public CharSequence subSequence(int beginIndex, int endIndex)
    {
        return this.substring(beginIndex, endIndex);
    }


    public String substring(int beginIndex)
    {
        if (beginIndex < 0) {
            throw new StringIndexOutOfBoundsException(beginIndex);
        }
        int subLen = value.length - beginIndex;
        if (subLen < 0) {
            throw new StringIndexOutOfBoundsException(subLen);
        }
        return (beginIndex == 0) ? this : new String(value, beginIndex, subLen);
    }


    public String substring(int beginIndex, int endIndex)
    {
        if (beginIndex < 0) {
            throw new StringIndexOutOfBoundsException(beginIndex);
        }
        if (endIndex > value.length) {
            throw new StringIndexOutOfBoundsException(endIndex);
        }
        int subLen = endIndex - beginIndex;
        if (subLen < 0) {
            throw new StringIndexOutOfBoundsException(subLen);
        }
        return ((beginIndex == 0) && (endIndex == value.length)) ? this
                : new String(value, beginIndex, subLen);
    }


    public String(char value[], int offset, int count)
    {
        if (offset < 0) {
            throw new StringIndexOutOfBoundsException(offset);
        }
        if (count < 0) {
            throw new StringIndexOutOfBoundsException(count);
        }
        if (offset > value.length - count) {
            throw new StringIndexOutOfBoundsException(offset + count);
        }

        this.value = new char[count];
        for (int i = 0; i < count; ++i) {
            this.value[i] = value[offset + i];
        }
    }


}

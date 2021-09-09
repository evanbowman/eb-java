package java.lang;



public final class StringBuilder implements CharSequence {


    private char[] data;
    private int count;


    private void grow(int newCapacity)
    {
        char[] newData = new char[newCapacity];

        if (data != null) {
            for (int i = 0; i < data.length; ++i) {
                newData[i] = data[i];
            }
        }

        data = newData;
    }


    private void ensureCapacity(int minCapacity)
    {
        if (data == null) {
            grow(minCapacity);
            return;
        }

        if (minCapacity > data.length) {
            if (minCapacity > data.length * 2) {
                grow(minCapacity);
            } else {
                grow(data.length * 2);
            }
        }
    }


    StringBuilder()
    {

    }


    public StringBuilder append(Object obj)
    {
        return append(obj.toString());
    }


    public StringBuilder append(String str)
    {
        char[] otherData = str.toCharArray();

        ensureCapacity(count + otherData.length);

        for (int i = 0; i < otherData.length; ++i) {
            data[count++] = otherData[i];
        }

        return this;
    }


    public StringBuilder append(int i)
    {
        return append(Integer.toString(i));
    }


    public StringBuilder append(char c)
    {
        ensureCapacity(count + 1);
        data[count++] = c;
        return this;
    }


    public StringBuilder append(boolean b)
    {
        return append(Boolean.toString(b));
    }


    public StringBuilder append(long l)
    {
        return append(Long.toString(l));
    }


    public String toString()
    {
        char[] result = new char[count];

        for (int i = 0; i < count; ++i) {
            result[i] = data[i];
        }

        return new String(result);
    }


    public String substring(int start)
    {
        return substring(start, count);
    }


    public String substring(int start, int end)
    {
        if (start < 0) {
            throw new StringIndexOutOfBoundsException(start);
        }
        if (end > count) {
            throw new StringIndexOutOfBoundsException(end);
        }
        if (start > end) {
            throw new StringIndexOutOfBoundsException(end - start);
        }
        return new String(data, start, end - start);
    }


    public CharSequence subSequence(int start, int end)
    {
        return substring(start, end);
    }


    public char charAt(int index)
    {
        if ((index < 0) || (index >= data.length)) {
            throw new StringIndexOutOfBoundsException(index);
        }
        return data[index];
    }


    public int length()
    {
        return count;
    }

}

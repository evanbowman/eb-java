package java.lang;



public final class StringBuilder {


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


    private void ensureCapacity(int capacity)
    {
        if (data == null) {
            grow(capacity);
            return;
        }

        int available = data.length - count;

        if (available <= capacity) {

            int newSize = data.length * 2;
            if (newSize < capacity) {
                newSize = capacity;
            }

            grow(newSize);
        }
    }


    StringBuilder()
    {

    }


    public StringBuilder append(String str)
    {
        char[] otherData = str.toCharArray();

        ensureCapacity(otherData.length);

        for (int i = 0; i < otherData.length; ++i) {
            data[count++] = otherData[i];
        }

        return this;
    }


    public StringBuilder append(int i)
    {
        return this;
    }


    public String toString()
    {
        char[] result = new char[count];

        for (int i = 0; i < count; ++i) {
            result[i] = data[i];
        }

        return new String(result);
    }
}

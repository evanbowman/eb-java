package java.lang;



public final class String {


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


    public boolean isEmpty()
    {
        return value.length == 0;
    }


    public char charAt(int index)
    {
        // if ((index < 0) || (index >= value.length)) {
        //     throw new StringIndexOutOfBoundsException(index);
        // }
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


}

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


    public int length()
    {
        return value.length;
    }
}

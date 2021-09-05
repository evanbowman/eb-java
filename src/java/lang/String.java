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


}

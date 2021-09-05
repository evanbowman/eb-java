package java.lang;



public class String {


    private final char data[];


    String(char[] data)
    {
        this.data = data.clone();
    }


}

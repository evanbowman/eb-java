package java.lang;


public class Throwable {


    private String message = null;
    private Throwable cause = null;


    public Throwable()
    {
    }


    public Throwable(String message)
    {
        this.message = message;
    }


    public Throwable(String message, Throwable cause)
    {
        this.message = message;
        this.cause = cause;
    }


}

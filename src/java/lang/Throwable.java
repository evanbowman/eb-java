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


    public Throwable(Throwable cause)
    {
        this.cause = cause;
    }


    public Throwable getCause()
    {
        return cause;
    }


    public String getMessage()
    {
        return message;
    }
}

package java.lang;



public class Throwable {


    public static boolean enableStackTraces = false;


    private String message = null;
    private Throwable cause = null;
    private StackTraceElement[] stackTrace = null;


    public Throwable()
    {
        if (enableStackTraces) {
            fillInStackTrace();
        }
    }


    public Throwable fillInStackTrace()
    {
        stackTrace = Runtime.getRuntime().stackTrace();
        return this;
    }


    public StackTraceElement[] getStackTrace()
    {
        return stackTrace;
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

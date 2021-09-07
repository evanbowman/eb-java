package java.lang;


public class Error extends Throwable {


    Error()
    {

    }


    Error(String message)
    {
        super(message);
    }


    Error(String message, Throwable cause)
    {
        super(message, cause);
    }


    Error(Throwable cause)
    {
        super(cause);
    }

}

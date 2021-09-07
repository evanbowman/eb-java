package java.lang;


class Exception extends Throwable {


    Exception()
    {

    }


    Exception(String message)
    {
        super(message);
    }


    Exception(String message, Throwable cause)
    {
        super(message, cause);
    }


    Exception(Throwable cause)
    {
        super(cause);
    }


}

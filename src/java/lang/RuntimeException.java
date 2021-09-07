package java.lang;


class RuntimeException extends Exception {


    RuntimeException()
    {

    }


    RuntimeException(String message)
    {
        super(message);
    }


    RuntimeException(String message, Throwable cause)
    {
        super(message, cause);
    }


    RuntimeException(Throwable cause)
    {
        super(cause);
    }


}

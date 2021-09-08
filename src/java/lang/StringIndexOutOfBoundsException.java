package java.lang;



public class StringIndexOutOfBoundsException extends RuntimeException {


    StringIndexOutOfBoundsException()
    {
    }


    StringIndexOutOfBoundsException(String message)
    {
        super(message);
    }


    StringIndexOutOfBoundsException(int index)
    {
        super("string index" + index + " out of bounds");
    }
}

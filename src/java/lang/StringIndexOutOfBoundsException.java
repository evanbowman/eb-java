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
        // FIXME: append index
        super("string index out of bounds");
    }
}

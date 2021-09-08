package java.lang;



public class ArrayIndexOutOfBoundsException extends RuntimeException {


    ArrayIndexOutOfBoundsException()
    {
    }


    ArrayIndexOutOfBoundsException(String message)
    {
        super(message);
    }


    ArrayIndexOutOfBoundsException(int index)
    {
        super("array index" + index + " out of bounds");
    }

}

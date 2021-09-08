package java.lang;



public class IndexOutOfBoundsException extends RuntimeException {


    public IndexOutOfBoundsException(int index)
    {
        super("Index " + index + " out of bounds");
    }


}

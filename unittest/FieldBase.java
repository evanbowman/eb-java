package test;



public class FieldBase {

    public int foo = 12;


    void verify()
    {
        if (foo != 12) {
            while (true) ;
        }

        foo = 16;
    }


    public static void main(String[] args)
    {
        // Nothing to do. We just created this class for another test case,
        // which assigns to a derived field of the same name, then calls
        // FieldBase.verify().
    }
}

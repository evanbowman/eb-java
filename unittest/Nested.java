package test;



class Nested {


    int i = 12;


    public class NestedTest {

        NestedTest()
        {
            if (i != 12) {
                Runtime.getRuntime().exit(1);
            }
        }
    }


    void test()
    {
        new NestedTest();
    }



    public static void main(String[] args)
    {
        (new Nested()).test();
    }
}

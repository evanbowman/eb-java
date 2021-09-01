package test;



class Parameters {
    // This test case makes sure that parameters are passed correctly to
    // functions.

    public static void test(int i, long l, char c, double d, short s)
    {
        if (i != 55 || c != 'A' || s != 11 || l != 894208493) {
            Runtime.getRuntime().exit(1);
        }
    }


    void foo(int i, float f)
    {
        if (i != 5 || f != 0.5f) {
            Runtime.getRuntime().exit(1);
        }
    }


    public static void main(String[] args)
    {
        Parameters p = new Parameters();

        short s = 11;


        test(55, 894208493, 'A', 0.345, s);


        p.foo(5, 0.5f);
    }

}

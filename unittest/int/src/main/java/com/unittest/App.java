package com.unittest;


// Unit test for primitive integers.


public class App
{
    public int var1 = 0;
    public int var2 = 1;


    public static void assert_cond(boolean cond)
    {
        if (!cond) {
            Runtime.getRuntime().exit(1);
        }
    }


    public static void main(String[] args)
    {
        App app = new App();

        int i = 1000;
        int j = app.var2;

        assert_cond(i + j == 1001);

        app.var1 += 1000;

        assert_cond(app.var1 == 1000);
        assert_cond(app.var2 == 1);

        app.var1 = i * 5 + j * 9;

        assert_cond(app.var1 == 5009);

        app.var1 -= 6;

        assert_cond(app.var1 == 5003);

        j = 2000;

        j /= 2;
        assert_cond(i == j);

        Runtime.getRuntime().exit(0);
    }
}

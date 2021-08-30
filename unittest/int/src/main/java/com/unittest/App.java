package com.unittest;



public class App
{
    public static void assert_cond(boolean cond)
    {
        if (!cond) {
            Runtime.getRuntime().exit(1);
        }
    }


    public static void main(String[] args)
    {
        App app = new App();

        int i = 0;
        int j = 1;

        assert_cond(i + j == 1);

        Runtime.getRuntime().exit(0);
    }
}

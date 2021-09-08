package test;



class Array {


    // Verify that array fields work correctly.
    public static int[] foo = new int[30];


    static
    {
        foo[4] = 10;
    }


    int call()
    {
        return 8;
    }


    // Pass an array as a parameter, make sure parameters are passed correctly
    // for array types.
    static void arrayParamTest(int a, Object[] b, char c)
    {
        if (a != 5 || ((Array)b[1]).call() != 8 || c != 'n') {
            Runtime.getRuntime().exit(1);
        }
    }


    public static void main(String[] args)
    {
        Object[] param = new Object[3];
        param[1] = new Array();


        arrayParamTest(5, param, 'n');

        if (foo[4] != 10) {
            Runtime.getRuntime().exit(1);
        }

        long[] longArray = new long[4];

        if (longArray.length != 4) {
            Runtime.getRuntime().exit(1);
        }

        longArray[3] = 5555;

        if (longArray[3] != 5555) {
            Runtime.getRuntime().exit(1);
        }

        try {
            longArray[10] = 5;
            Runtime.getRuntime().exit(1);
        } catch (ArrayIndexOutOfBoundsException exn) {
            // ...
        } catch (Throwable t) {
            Runtime.getRuntime().exit(1);
        }

        int[] intArray = new int[100];

        intArray[0] = 3;

        if (intArray[0] != 3) {
            Runtime.getRuntime().exit(1);
        }

        byte[] barray = new byte[1024];
        barray[0] = -1;

        if (barray[0] != -1) {
            Runtime.getRuntime().exit(1);
        }

        for (int i = 0; i < intArray.length; ++i) {
            intArray[i] = i;
        }

        int[] ia2 = intArray.clone();

        if (intArray.length != ia2.length) {
            Runtime.getRuntime().exit(1);
        }

        for (int i = 0; i < ia2.length; ++i) {
            if (ia2[i] != intArray[i]) {
                Runtime.getRuntime().exit(1);
            }
        }

        ia2[5] = 1000;

        if (ia2[5] == intArray[5]) {
            Runtime.getRuntime().exit(1);
        }
    }

}

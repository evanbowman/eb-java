package test;



class Array {


    public static void main(String[] args)
    {
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
        } catch (Throwable exn) {
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
    }

}

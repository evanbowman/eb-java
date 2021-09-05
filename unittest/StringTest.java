package test;



class StringTest {


    public static void main(String[] args)
    {
        String str = "cat";

        Runtime.getRuntime().gc();

        if (str.length() != 3) {
            Runtime.getRuntime().exit(1);
        }

        if (str.charAt(0) != 'c' ||
            str.charAt(1) != 'a' ||
            str.charAt(2) != 't') {
            Runtime.getRuntime().exit(1);
        }
    }


}

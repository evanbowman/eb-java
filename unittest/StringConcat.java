package test;



public class StringConcat {


    public static void main(String[] args)
    {
        StringBuilder builder = new StringBuilder();
        builder.append("hello, ");
        builder.append("world!");

        if (!builder.toString().equals("hello, world!")) {
            Runtime.getRuntime().exit(1);
        }


        // TODO: Java 9 onwards uses invokedynamic to concatenate strings. I
        // added the stringbuilder concatenation test (above) for when I
        // eventually finish implementing invokedynamic.
        String str1 = "foo";
        String str2 = "bar";

        if (!(str1 + str2).equals("foobar")) {
            Runtime.getRuntime().exit(1);
        }
    }


}

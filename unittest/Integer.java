package test;



class Integer {


    public static void main(String args[])
    {
        int i = 5;

        try {
            i = i / 0;
            Runtime.getRuntime().exit(1);
        } catch (ArithmeticException e) {
            // TODO: use the correct class in catch expression
        } catch (Throwable t) {
            Runtime.getRuntime().exit(1);
        }


        if (!java.lang.Integer.toString(345).equals("345")) {
            Runtime.getRuntime().exit(1);
        }


        if (java.lang.Integer.parseInt("505") != 505) {
            Runtime.getRuntime().exit(1);
        }
    }


}

package test;



class Integer {


    public static void main(String args[])
    {
        int i = 5;

        try {
            i = i / 0;
            Runtime.getRuntime().exit(1);
        } catch (Throwable t) {
            // TODO: use the correct class in catch expression
        }


    }


}

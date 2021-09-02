package test;



class Static {

    int foo = 0;
    static int bar = 12;
    int baz = 0;

    static Static s = null;


    static {
        s = new Static();
        s.baz = 1;
    }


    static void main(String[] args)
    {
        if (bar != 12) {
            // Static block didn't run.
            Runtime.getRuntime().exit(1);
        }

        bar = 2;

        s.bar += s.baz;

        if (bar != 3) {
            Runtime.getRuntime().exit(1);
        }
    }


}

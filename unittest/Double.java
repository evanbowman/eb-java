package test;



class Double {


    public static void main(String[] args)
    {
        double d1 = 5.0;
        double d2 = 4.0;

        if (d1 < d2) {
            Runtime.getRuntime().exit(1);
        }

        if (d1 < 2.0) {
            Runtime.getRuntime().exit(1);
        }

        d1 += d2;

        if (d1 != 9.0) {
            Runtime.getRuntime().exit(1);
        }

        d2 -= 3.0;

        if (d2 != 1.0) {
            Runtime.getRuntime().exit(1);
        }

        if (3.0 / 2.0 != 1.5) {
            Runtime.getRuntime().exit(1);
        }

        int i = (int)d2;

        if (i != 1) { // TODO: is this actually guaranteed to be a reliably
                      // precise conversion/?
            Runtime.getRuntime().exit(1);
        }
    }


}

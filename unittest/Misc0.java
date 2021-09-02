package test;


// In addition to specific test cases, I'm creating a number of arbitrary test
// cases with increasingly convoluted structure, to add coverage for certain
// obscure scenarios.


class Misc0 {

    public Object test = null;
    public char t2 = 'a';

    public int foo(int i, int j, long m, int k) throws Throwable {

        if (i + j + k != 3) {
            Runtime.getRuntime().exit(1);
        } else {
            int z = 30;
            if (j == 1) {
                throw new Throwable();
            }
            return j + z;
        }

        return 0;
    }

    public static void main(String args[]) throws Throwable {
        int test = 0;
        int test2 = 1;
        int test3 = 6400;

        Misc0 t2 = new Misc0();
        t2.test = t2;

        while (test < 100000) {
            if (test > 5000) {
                try {
                    try {
                        ((Misc0)t2.test).foo(0, 1, 0, 2);
                    } catch (Throwable t) {
                        return;
                    }
                } catch (Throwable t) {
                    Runtime.getRuntime().exit(1);
                }
            }

            test += test3;
        }

        Runtime.getRuntime().exit(1);
    }
}

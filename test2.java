package test;


class Test2 {

    public Object test = null;

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

        Test2 t2 = new Test2();
        t2.test = t2;

        while (test < 100000) {
            try {
                try {
                    ((Test2)t2.test).foo(0, 1, 0, 2);
                } catch (Throwable t) {
                    Runtime.getRuntime().exit(1);
                }
            } catch (Throwable t) {
                while (true) {
                    // ...
                }
            }



            test += test3;
        }

        Runtime.getRuntime().exit(0);
    }
}

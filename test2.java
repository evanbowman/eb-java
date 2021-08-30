package test;


class Test2 {

    public Object test = null;

    public int foo(int i, int j, long m, int k) {
        if (i + j + k != 3) {
            Runtime.getRuntime().exit(1);
        }
        return 0;
    }

    public static void main(String args[]) {
        int test = 0;
        int test2 = 1;
        int test3 = 6400;

        Test2 t2 = new Test2();
        t2.test = t2;

        while (test < 100000) {
            ((Test2)t2.test).foo(0, 1, 0, 2);

            test += test3;
        }

        Runtime.getRuntime().exit(0);
    }
}

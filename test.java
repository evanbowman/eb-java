package test;



class HelloWorldApp extends Example {

    public short foo = 0;
    public int bar = 0;
    public float baz = 10.f;
    public char c = 'A';


    public HelloWorldApp fn() {
        return this;
    }


    public static void test(int i) {

    }

    public static void main(String[] args) {

        HelloWorldApp app = new HelloWorldApp();

        int i = 0;
        int j = 1;
        int k = 1;
        int l = 1;

        while (true) {

            test(i++);
            app.foo++;
            app.bar++;
            app.baz++;

            if (app.foo > 20) {
                app.fn();
            }
        }
    }
}

package test;



class HelloWorldApp extends Example {

    public short foo = 0;
    public int bar = 0;
    public float baz = 10.f;
    public char c = 'A';


    public Object fn() {
        return (Object)this;
    }


    public static void test(int i) {

    }

    public static void main(String[] args) {

        HelloWorldApp app = new HelloWorldApp();

        int i = 0;
        int j = 40000;
        int k = 2;
        int l = j / k;
        float test = 8.f;

        while (true) {
            test(i++);
            app.foo++;
            app.bar++;
            app.baz++;
            app.membr--;

            if (app.foo > 20) {
                Object result = app.fn();
                if (result instanceof Object) {
                    ((HelloWorldApp)result).test(0);
                }
            } else {
                app.testfn();
            }
        }
    }
}

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

        float[] t2 = new float[3];
        t2[1] = 55.f;
        if (t2[1] == 55.f) {
            t2[1] = 65.f;
        }

        Object[] test2 = new Object[10];

        while (true) {
            test(i++);
            app.foo++;
            app.bar++;
            app.baz++;
            app.membr--;

            if (app.bar < 10) {
                Object result = app.fn();
                if (result instanceof Object) {
                    ((HelloWorldApp)result).test(0);
                }
            } else {
                int data[] = app.testfn();
                app.membr = data.length;

                data[0] = 1;

                while (data[0] != 1) {

                }
            }
        }
    }
}

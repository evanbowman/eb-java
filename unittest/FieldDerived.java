package test;



class FieldDerived extends FieldBase {

    private int foo = 5;


    public static void main(String[] args)
    {
        FieldDerived f = new FieldDerived();

        f.foo = 8;

        // NOTE: we created a private variable foo, which shadows foo from the
        // derived FieldBase. Verify that we don't overwrite anything in the
        // base class.
        f.verify();


        if (f.foo != 8) {
            Runtime.getRuntime().exit(1);
        }
    }
}

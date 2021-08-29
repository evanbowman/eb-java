package test;


interface Foo {
    void imethod();
}


class Example implements Foo {
    public int membr = 0;

    public int[] testfn() {
        Foo f = (Foo)this;
        f.imethod();

        return new int[100];
    }

    public void imethod() {

    }
}

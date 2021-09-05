package test;



interface MyTestInterface {

    void notifyStuff();

}


class InterfaceTest implements MyTestInterface {

    public int i = 0;


    public static void main(String[] args)
    {
        try {
            Object obj = new InterfaceTest();
            ((MyTestInterface)obj).notifyStuff();
            if (((InterfaceTest)obj).i != 1) {
                Runtime.getRuntime().exit(1);
            }
        } catch (Throwable t) {
            Runtime.getRuntime().exit(1);
        }
    }


    public void notifyStuff()
    {
        i = 1;
    }
}

package test;



class Throw {


    public static void makeExn() throws Throwable
    {
        throw new RuntimeException("hello, world!");
    }


    public static void main(String[] args)
    {
        try {
            makeExn();
            Runtime.getRuntime().exit(1);
        } catch (RuntimeException r) {
            if (!r.getMessage().equals("hello, world!")) {
                Runtime.getRuntime().exit(1);
            }
        } catch (Throwable t) {
            Runtime.getRuntime().exit(1);
        }
    }


}

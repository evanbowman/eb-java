package java.lang;



public class Runtime {


    private static Runtime runtime = null;


    static {
        runtime = new Runtime();
    }


    public static Runtime getRuntime()
    {
        return runtime;
    }


    public native void exit(int code);


    public native void gc();


    public native long totalMemory();


    public native long freeMemory();


    public void halt(int code)
    {
        exit(code);
    }


    int availableProcessors()
    {
        return 1;
    }


    public long maxMemory()
    {
        return totalMemory();
    }
}

package java.lang;



public class Runtime {


    private static Runtime runtime = null;


    private Runtime() {}


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


    public native StackTraceElement[] stackTrace();


    public native void debug();


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


    public void traceInstructions(boolean on)
    {
    }


    public void traceMethodCalls(boolean on)
    {
    }


}

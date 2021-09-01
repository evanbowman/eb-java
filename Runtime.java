package java.lang;



class Runtime {


    public static native Runtime getRuntime();


    public native void exit(int code);


    public native void gc();


    public native long totalMemory();


    public native long freeMemory();

}

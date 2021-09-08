package java.lang;



public final class StackTraceElement {

    private final String declaringClass;
    private final String methodName;


    public StackTraceElement(String declaringClass,
                             String methodName,
                             String fileName,
                             int lineNumber)
    {
        this.declaringClass = declaringClass;
        this.methodName = methodName;
    }


    public String getFileName()
    {
        return null;
    }


    public int getLineNumber()
    {
        return 0;
    }


    public String getClassName()
    {
        return declaringClass;
    }


    public String getMethodName()
    {
        return methodName;
    }


    public String toString()
    {
        return declaringClass + "." + methodName;
    }
}

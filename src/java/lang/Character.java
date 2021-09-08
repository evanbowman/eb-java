package java.lang;



public final class Character implements Comparable<Character> {


    private char value;


    public static Character valueOf(char c)
    {
        return new Character(c);
    }


    public static char toUpperCase(char ch)
    {
        if (ch >= 'a' && ch <= 'z') {
            return (char)(ch - 32);
        } else {
            return ch;
        }
    }


    public static char toLowerCase(char ch)
    {
        if (ch >= 'A' && ch <= 'Z') {
            return (char)(ch + 32);
        } else {
            return ch;
        }
    }


    public int compareTo(Character other)
    {
        return this.value - other.value;
    }


    public static String toString(char c)
    {
        char buf[] = {c};
        return String.valueOf(buf);
    }


    public String toString()
    {
        return toString(value);
    }


}

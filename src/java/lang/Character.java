package java.lang;



public final class Character implements Comparable<Character> {


    private final char value;


    public Character(char value)
    {
        this.value = value;
    }


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
        return compare(this.value, other.value);
    }


    public static int compare(char x, char y)
    {
        return x - y;
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


    public static int digit(char ch, int radix)
    {
        int value = -1;
        if (radix >= 2 && radix <= 36) {
            // FIXME...
            if (ch >= '0' && ch <= '9') {
                return ch - 48;
            }
        }
        return (value < radix) ? value : -1;
    }

}

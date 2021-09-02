package test;



public class Switch {


    public static void main(String[] args)
    {
        int i = 2;

        switch (i) {
        case 0:
            i = 5;
            break;

        case 1:
        case 2:
            i = 7;
            break;

        default:
            i = 12;
            break;
        }

        if (i != 7) {
            Runtime.getRuntime().exit(1);
        }



        i = 9999;
        switch (i) {
        case 9999:
            i = 15;
            break;

        case -100:
            i = 12;
            break;

        default:
            i = 4;
            break;
        }

        if (i != 15) {
            Runtime.getRuntime().exit(1);
        }
    }

}

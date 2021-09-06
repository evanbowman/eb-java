package test;



class MultiArray {



    public static void main(String[] args)
    {
        Object blah = new Object();

        int[][][] test = new int[10][5][3];

        int counter = 0;

        for (int i = 0; i < 10; ++i) {
            for (int j = 0; j < 5; ++j) {
                for (int k = 0; k < 3; ++k) {
                    test[i][j][k] = counter;
                    ++counter;
                }
            }
        }

        blah = null;
        // Running the gc will collect the var that was bound to blah (above),
        // thus shifting the multidimensional array in memory during
        // compaction. Check the sum of the cube below to make sure that it adds
        // up after being shifted.
        Runtime.getRuntime().gc();

        counter = 0;
        for (int i = 0; i < 10; ++i) {
            for (int j = 0; j < 5; ++j) {
                for (int k = 0; k < 3; ++k) {
                    counter += test[i][j][k];
                }
            }
        }

        if (counter != 11175) {
            Runtime.getRuntime().exit(1);
        }
    }



}

# Running unit tests

Simply run the script `runtests.sh` from this directory after building the java executable in the parent directory.

Each java class in this directory should implement a main method, which performs testing. I am currently using `Runtime.getRuntime().exit(1)` to indicate unit test failures, because doing so was the easiest way to create unit tests without bootstrapping tons of JRE classes.

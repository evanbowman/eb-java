
make
javac --release 8 game/startup/*.java
javac --release 8 game/tetris/*.java
jar cf0 Test.jar game
cat JRE.gba Test.jar > Test.gba

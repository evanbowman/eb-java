rm Lang.jar
javac --release 8 -Xdiags:verbose java/lang/*.java
javac --release 8 -Xdiags:verbose java/util/*.java
cp java/lang/*.class pkg/java/lang
cp java/util/*.class pkg/java/util
cd pkg
zip -0 -r Lang.jar ./*
mv Lang.jar ../
cd ..

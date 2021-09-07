javac --release 8 java/lang/*.java
cp java/lang/*.class pkg/java/lang
cd pkg
zip -0 -r Lang.jar ./*
mv Lang.jar ../
cd ..

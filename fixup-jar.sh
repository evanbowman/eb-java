
# Our jvm cannot handle any compression in jar files. The jar needs to be
# decompressed and recompressed.
rm -r test-contents
unzip TEST.jar -d test-contents
cd test-contents
zip -0 -r TEST2.jar ./*
cp TEST2.jar ../
cd -

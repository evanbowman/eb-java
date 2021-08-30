

for dir in ./*/
do
    dir=${dir%*/}

    echo Entering dir $dir...

    cd $dir
    echo building jar for $dir
    echo Building test jar...
    mvn clean install -DskipTests > /dev/null

    echo Unpacking test jar...
    unzip target/test-1.0-SNAPSHOT.jar -d ./tmp/ > /dev/null
    cd ./tmp/
    echo Repacking test jar without compression...
    zip -0 -r fixup.jar ./* > /dev/null
    cp fixup.jar ../
    cd ..

    rm -r ./tmp/

    echo Starting jvm...
    gdb ../../java

    rm fixup.jar

    cd ..
    pwd

done



for dir in ./*/
do
    dir=${dir%*/}

    echo Entering dir $dir...

    cd $dir
    echo building jar for $dir
    echo Building test jar...
    mvn clean install -DskipTests > build-log.txt

    echo Unpacking test jar...
    unzip target/test-1.0-SNAPSHOT.jar -d ./tmp/ > /dev/null
    cd ./tmp/
    echo Repacking test jar without compression...
    zip -0 -r fixup.jar ./* > /dev/null
    cp fixup.jar ../
    cd ..

    rm -r ./tmp/

    echo Starting jvm...
    ../../java fixup.jar com/unittest/App
    exit_status=$?
    if [ $exit_status -eq 1 ]; then
        echo unit test failed!
        exit 1
    fi


    rm fixup.jar

    cd ..
    pwd

done

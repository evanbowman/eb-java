

for dir in ./*/
do
    dir=${dir%*/}
    cd $dir
    echo building jar for $dir
    mvn clean install -DskipTests

    unzip target/test-1.0-SNAPSHOT.jar -d ./tmp/
    cd ./tmp/
    zip -0 -r fixup.jar ./*
    cp fixup.jar ../
    cd ..

    rm fixup.jar

    cd -
done

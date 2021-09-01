
javac *.java
mv *.class pkg/test/
cd pkg
zip -0 -r Test.jar ./*
mv Test.jar ../
cd ..


for i in *.java; do
    [ -f "$i" ] || break

    echo ================================================================================
    echo Running test $i...
    echo ================================================================================

    ../java Test.jar test/"${i%%.*}"

    exit_status=$?

    if [ $exit_status -eq 1 ]; then
        echo unit test $i failed!
        exit 1
    fi

    echo Test success!
    echo ""
done

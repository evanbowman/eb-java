
javac *.java
mv *.class test/
jar cf0 Test.jar test


for i in *.java; do
    [ -f "$i" ] || break

    echo ================================================================================
    echo Running test $i...
    echo ================================================================================

    ../eb-java Test.jar test/"${i%%.*}"

    exit_status=$?

    if [ $exit_status -eq 1 ]; then
        echo unit test $i failed!
        exit 1
    fi

    echo Test success!
    echo ""
done

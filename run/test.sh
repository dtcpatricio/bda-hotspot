#!/bin/sh

case "$1" in
    -internal)
        TEST=internalvmtests
        shift
        ;;
    -client)
        TEST=clienttest
        shift
        ;;
esac

TEST_MAKEFILE_PATH=`cd ../test && pwd`
TEST_MAKEFILE=$TEST_MAKEFILE_PATH/Makefile

ALT_JVM_PATH=`cd ../build/linux/linux_amd64_compiler2/debug && pwd`

export JAVA_HOME=`cd /usr/lib/jvm/java-8-openjdk && pwd`
export JAVA_ARGS="-Dsun.java.launcher=gamma -XXaltjvm=$ALT_JVM_PATH"
export PRODUCT_HOME=$JAVA_HOME
export SLASH_JAVA=$JAVA_HOME
export JT_HOME=`cd ../../jtreg && pwd`

exec make --file=$TEST_MAKEFILE hotspot_$TEST

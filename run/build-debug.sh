#!/bin/sh

JAVA_HOME=`cd /usr/lib/jvm/java-8-openjdk && pwd`;
export JAVA_HOME

BUILD_SCRIPT_DIR=`cd ../make && pwd`;
BUILD_SCRIPT=$BUILD_SCRIPT_DIR/build.sh

BUILD_DIR=`cd ../build/linux && pwd`

case "$1" in
    -debug)
        MODE=debug
        shift
        ;;
    -clean)
        MODE=clean
        shift
        ;;
    -fastdebug)
        MODE=fastdebug
        shift
        ;;
    *)
        MODE=
        ;;
esac


exec $BUILD_SCRIPT $MODE LP64=1 DEBUG_BINARIES=true ALT_OUTPUTDIR=$BUILD_DIR

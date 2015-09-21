#!/bin/sh

JAVA_HOME=`cd /usr/lib/jvm/java-8-openjdk && pwd`;
export JAVA_HOME

BUILD_SCRIPT_DIR=`cd ../make && pwd`;
BUILD_SCRIPT=$BUILD_SCRIPT_DIR/build.sh

BUILD_DIR=`cd ../build/linux && pwd`

while :
do
    case "$1" in
        -debug)
            MODE=debug
            shift
            ;;
        -clean)
            MODE=clean
            shift
            ;;
        -header)
            ENABLE_BDA_HEADERMARK=1
            ENABLE_BDA_HASHMARK=0
            shift
            ;;
        -hash)
            ENABLE_BDA_HASHMARK=1
            ENABLE_BDA_HEADERMARK=0
            shift
            ;;
        -product)
            MODE=product
            shift
            ;;
        *)
            MODE=product
            break
            ;;
    esac
done

exec $BUILD_SCRIPT $MODE LP64=1 DEBUG_BINARIES=true ALT_OUTPUTDIR=$BUILD_DIR ENABLE_BDA_HEADER_MARK=$ENABLE_BDA_HEADERMARK ENABLE_BDA_HASH_MARK=$ENABLE_BDA_HASHMARK

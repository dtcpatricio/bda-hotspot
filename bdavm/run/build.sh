#!/bin/sh

JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64;

BUILD_SCRIPT_DIR=`cd ../make && pwd`;
BUILD_SCRIPT=$BUILD_SCRIPT_DIR/build.sh

BUILD_DIR="$(dirname $0)/../build/linux"
DEBUG_BIN=false

# Print help and exit
help () {
    echo -e "Usage:"
    exit !?
}

while :
do
    case "$1" in
        -h|--help)
            help
            ;;
        -buildir)
            if [ ! -d "$2" ]; then
                mkdir -p "$2"
            fi
            BUILD_DIR=`cd $2 && pwd`
            shift 2
            ;;
	-bootdir)
	    JAVA_HOME=`cd $2 && pwd`
 	    shift 2
	    ;;
	-mode)
	    MODE=$2
	    shift 2
	    ;;
        -debugbin)
            DEBUG_BIN=1
            shift
            ;;
        -bda)
            ENABLE_BDA=1
            shift
            ;;
        -bda-interpreter-only)
            ENABLE_BDA_INTERPRETER_ONLY=1
            shift
            ;;
        -jobs)
            HOTSPOT_BUILD_JOBS=$2
            shift 2
            ;;
        -asm)
            NEED_ASM=1
            shift 1
            ;;
        *)
            break
            ;;
    esac
done

export JAVA_HOME

exec $BUILD_SCRIPT $MODE LP64=1 DEBUG_BINARIES=$DEBUG_BIN ALT_OUTPUTDIR=$BUILD_DIR ENABLE_BDA_INTERPRETER_ONLY=$ENABLE_BDA_INTERPRETER_ONLY ENABLE_BDA=$ENABLE_BDA HOTSPOT_BUILD_JOBS=$HOTSPOT_BUILD_JOBS NEED_ASM=$NEED_ASM

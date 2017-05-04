#!/bin/bash

# This script launches HotSpot from the build directory using the hotspot script
# inside the build directory.

# TODO: Create another way of launching the vm.

# Pull options from launcher-include. All VM options should go in there
# Additional options for the command-line are available with -X[..].
source launch-include.sh

help () {
    echo "Usage:"
    echo -e "-gud \t\t launch the hotspot script in a emacs-gud window. If omitted it runs normally"
    echo -e "-vm [directory] \t\t Specify the directory of the libjvm.so (obligatory)"
    echo -e "-jar exec [arguments] \t\t If specified, the executable, with optional arguments, must exit"
    echo -e "-cp | --classpath \t\t If no jar is used, specify the classpath string"
}

while :
do
    case "$1" in
        -gud)
             mode=-gud
             shift
             ;;
        -h|--help)
            help;
            exit 0;
            ;;
        -cp | --classpath)
            classpath="$2"
            shift 2
            ;;
        -jhome)
            jhome="$2"
            shift 2
            ;;
        -jar)
            jarfile="$2"
            if [ -z "$3" ]; then
                exit -1;
            fi
            prog="$3"
            arguments="${@:4:$#}"
            break
            ;;
        -vm)
            HS_SCRIPT_DIR=`cd $2 && pwd`
            shift 2
            ;;
        --)
            break
            ;;
        -*)
            add_args="${add_args} $1"
            shift
            ;;
        *)
            prog="$1"
            arguments="${@:2:$#}"
          break
          ;;
    esac
done

if [ ! -z $JARFILE ] ; then
    JAVA_ARGS="-jar $JARFILE $prog $arguments"
else
    JAVA_ARGS="-classpath $classpath $prog $arguments"
fi

ALT_JAVA_HOME=$jhome

# Transport these to the following script
export JAVA_ARGS
export ALT_JAVA_HOME

${HS_SCRIPT_DIR}/hotspot ${mode} $JVM_ARGS

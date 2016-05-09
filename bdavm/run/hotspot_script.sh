#!/bin/bash

while :
do
    case "$1" in
        -h|--help)
            echo "Usage:"
            echo -e "-bda \t\t <Ratio for the BDA-Spaces under the overall Old Space> <Class names as strings on a comma separated list>"
            echo -e "-nocomp \t Don't use compressed references and Class pointers"
            echo -e "-dir \t\t Specify the directory of the libjvm.so (obligatory)"
            echo -e "-printgc \t Set printing the BDA-Region's occupancy after FullGC"
            echo -e "-printminorgc \t Set printing the BDA-Region's occupancy after MinorGC"
            echo -e "-dacapo \t <.jar file> <Test to Run> <Number of Iterations as nX where X is an int => 1> Run with the DaCapo benchmark suite"
            exit 1;
            ;;
        -bda)
            usebda="-XX:+UseBDA -XX:BDARegionRatio=$2 -XX:BDAKlasses=${@:3:1}"
            shift 3
            ;;
        -dacapo)
            DACAPO_ARGS="-jar ${@:2:3}"
            shift 4
            ;;
        -nocomp)
            compressed="-XX:-UseCompressedOops -XX:-UseCompressedClassPointers"
            shift
            ;;
        -printgc)
            print="${print} -XX:+BDAPrintAfterGC"
            shift
            ;;
        -printminorgc)
            print="${print} -XX:+BDAPrintAfterMinorGC"
            shift
            ;;
        -dir)
            HS_SCRIPT_DIR=`cd $2 && pwd`
            shift 2
            ;;
        -*)
            add_args="${add_args} $1"
            shift
            ;;
        *)
          prog=$1
          break
          ;;
    esac
done

REL_MYDIR=`dirname $0`
MYDIR=`cd $REL_MYDIR && pwd`

if [ -z "$DACAPO_ARGS" ]; then
    JAVA_ARGS="-classpath $MYDIR $prog"
else
    JAVA_ARGS="-classpath $MYDIR $DACAPO_ARGS"
fi

JVM_ARGS="-XX:-UsePerfData -XX:-UseAdaptiveSizePolicy ${usebda} ${compressed} ${print} ${add_args}"

echo "JAVA_ARGS to be used $JAVA_ARGS"
echo "JVM_ARGS to be used $JVM_ARGS"
echo "Using hotspot dir $HS_SCRIPT_DIR"

ALT_JAVA_HOME=$JAVA_HOME

export JAVA_ARGS
export ALT_JAVA_HOME

exec $HS_SCRIPT_DIR/hotspot -gud $JVM_ARGS

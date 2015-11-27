#!/bin/sh

#HS_SCRIPT_DIR=`cd ../build/linux/linux_amd64_compiler2/product && pwd`

while :
do
    case "$1" in
        -bda)
            usebda="-XX:+UseBDA"
            shift
            ;;
        -dacapo)
            DACAPO_ARGS="-jar ${@:2:3}"
            shift 4
            ;;
        -nocomp)
            compressed="-XX:-UseCompressedOops -XX:-UseCompressedClassPointers"
            shift
            ;;
        -print)
            print="-XX:+BDAPrintRegions"
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

JVM_ARGS="${usebda} ${compressed} ${print} ${add_args}"

echo "JAVA_ARGS to be used $JAVA_ARGS"
echo "JVM_ARGS to be used $JVM_ARGS"
echo "Using hotspot dir $HS_SCRIPT_DIR"

ALT_JAVA_HOME=`cd /usr/lib/jvm/java-8-openjdk && pwd`

export JAVA_ARGS
export ALT_JAVA_HOME

exec $HS_SCRIPT_DIR/hotspot -gud $JVM_ARGS

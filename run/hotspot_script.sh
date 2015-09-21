#!/bin/sh

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
            compresed="-XX:-UseCompressedOops -XX:-UseCompressedClassPointers"
            shift
            ;;
        -print)
            print="-XX:+BDAPrintRegions"
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
HS_SCRIPT_DIR=`cd ../build/linux/linux_amd64_compiler2/product && pwd`

if [ -z "$DACAPO_ARGS" ]; then
    JAVA_ARGS="-classpath $MYDIR $prog"
else
    JAVA_ARGS="-classpath $MYDIR $DACAPO_ARGS"
fi

JVM_ARGS="${usebda} ${compressed} ${print}"

echo "JAVA_ARGS to be used $JAVA_ARGS"
echo "JVM_ARGS to be used $JVM_ARGS"

ALT_JAVA_HOME=`cd /usr/lib/jvm/java-8-openjdk && pwd`

export JAVA_ARGS
export ALT_JAVA_HOME

exec $HS_SCRIPT_DIR/hotspot -gud $JVM_ARGS

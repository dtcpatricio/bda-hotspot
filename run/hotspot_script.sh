#!/bin/sh

while [[ "$#" -gt 0 ]]
do
    key="$1"

    case $key in
        -dacapo)
            DACAPO_ARGS="-jar $2 $3"
		##JVM_ARGS="-XX:+PrintGCDetails -XX:+PrintGCTimeStamps"
            shift
            ;;
        -experimental)
            JVM_ARGS="-XX:+UnlockExperimentalVMOptions -XX:+UseBDA"
            shift
            ;;
        -bdadacapo)
            DACAPO_ARGS="-jar $2 $3"
            JVM_ARGS="-XX:+UseBDA -Xmx900m"
            shift
            ;;
	-nocomp)
		RUN_PROG=$2
		JVM_ARGS="-XX:-UseCompressedOops -XX:-UseCompressedClassPointers"
		shift
		;;
        *)
            RUN_PROG=$1
		JVM_ARGS="-XX:+UnlockExperimentalVMOptions -XX:+UseBDA"
		##JVM_ARGS="-XX:+PrintGCDetails -XX:+PrintGCTimeStamps -XX:+TraceParallelOldGCSummaryPhase"
            shift
            ;;
    esac
    shift
done

REL_MYDIR=`dirname $0`
MYDIR=`cd $REL_MYDIR && pwd`

SCRIPT_DIR=`cd ../build/linux/linux_amd64_compiler2/debug && pwd`
if [ -z "$DACAPO_ARGS" ]; then
    JAVA_ARGS="-classpath $MYDIR $RUN_PROG"
else
    JAVA_ARGS="-classpath $MYDIR $DACAPO_ARGS"
fi

echo "JAVA_ARGS to be used $JAVA_ARGS"

ALT_JAVA_HOME=`cd /usr/lib/jvm/java-8-openjdk && pwd`

export JAVA_ARGS
export ALT_JAVA_HOME

exec $SCRIPT_DIR/hotspot -gud $JVM_ARGS

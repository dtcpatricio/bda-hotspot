#!/bin/sh

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
heapdump_dir="${LHOME}/HeapDumps/"

while :
do
    case "$1" in
        -local)
            bench=local
            shift
            break
            ;;
        -external)
            bench=external
            shift
            break
            ;;
        -heapdump)
            ## This option runs the benchmark/test in the background
            ## and calls jmap periodically, according to the frequency
            ## argument, over the test.
            heapdump=1
            frequency="$2"
            shift 2
            ;;
        -vm)
            VM_SO_DIR="$2"
            shift 2
            ;;
        -jhome)
            ALT_JAVA_HOME="$2"
            shift 2
            ;;
        -bda)
            usebda="-XX:+UseBDA"
            ratio="-XX:BDARatio=$2"
            classes="-XX:BDAKlasses=$3"
            shift 3
            ;;
        -b)
           # Runs on the background
           background=1
           shift 1
           ;;
        -*)
            add_args="${add_args} $1"
            shift
            ;;
        *)
            break
            ;;
    esac
done

if [ ! -z "$ALT_JAVA_HOME" ]; then
    JAVA_HOME=${ALT_JAVA_HOME}
    export JAVA_HOME
fi

# FIXME: Not portable.
if [ -z "$VM_SO_DIR" ] ; then
    VM_SO_DIR="$JAVA_HOME/lib/amd64/server"
fi

JDK=${JAVA_HOME}
VMPARMS="-Dsun.java.launcher=gamma -XXaltjvm=$VM_SO_DIR"
LAUNCHER=$JDK/bin/java
if [ ! -x $LAUNCHER ] ; then
    echo "Error: Cannot find the java launcher $LAUNCHER"
    exit 1
fi


### JVM OPTIONS
## Most are obligatory therefore do not disable them for the sake of the benchmarks.
## The ones with the possibility of being disabled are noted for such.

## GC and Metadata Options
JVM_OPTS="$JVM_OPTS ${usebda} ${ratio} ${classes}"
JVM_OPTS="$JVM_OPTS -XX:+UseParallelGC"
JVM_OPTS="$JVM_OPTS -XX:+UseParallelOldGC"
JVM_OPTS="$JVM_OPTS -XX:-UseAdaptiveSizePolicy"
JVM_OPTS="$JVM_OPTS -XX:-UsePerfData"
JVM_OPTS="$JVM_OPTS -XX:-UseCompressedOops"
JVM_OPTS="$JVM_OPTS -XX:-UseCompressedClassPointers"

## Print Options (these can be commented out)
JVM_OPTS="$JVM_OPTS -XX:+PrintGC"
#JVM_OPTS="$JVM_OPTS -XX:+PrintGCDetails"
#JVM_OPTS="$JVM_OPTS -XX:+PrintGCDateStamps"
#JVM_OPTS="$JVM_OPTS -XX:+PrintHeapAtGC"
JVM_OPTS="$JVM_OPTS -Xloggc:/media/storage2/GCLogs/${bench}.gclog"
#JVM_OPTS="$JVM_OPTS -XX:+PrintGCApplicationStoppedTime"
#JVM_OPTS="$JVM_OPTS -XX:+PrintEnqueuedContainers"
#JVM_OPTS="$JVM_OPTS -XX:BDAllocationVerboseLevel=2"
#JVM_OPTS="$JVM_OPTS -XX:+BDAContainerFragAtFullGC"
#JVM_OPTS="$JVM_OPTS -XX:+PrintBDAContentsAtFullGC"

# Override options
JVM_OPTS="$JVM_OPTS ${add_args}"

echo "JVM_OPTS to be used ${JVM_OPTS}"
echo "Using hotspot lib ${VM_SO_DIR}"

### JAVA OPTIONS
if [ "${bench}" = "local" ] ; then
    JAVA_ARGS="-classpath $SCRIPTPATH/Microbenchmark $@"
    echo "JAVA_ARGS to be used ${JAVA_ARGS}"
    if [ -z ${background} ] && [ -z ${heapdump} ] ; then
        $LAUNCHER $VMPARMS $JVM_OPTS $JAVA_ARGS
    else
        nohup $LAUNCHER $VMPARMS $JVM_OPTS $JAVA_ARGS \
              > ${bench}.out 2> ${bench}.err < /dev/null &
        echo $! > ${bench}.pid
    fi
elif [ "${bench}" = "external" ] ; then
    JAVA_ARGS="$@"
    echo "JAVA_ARGS to be used ${JAVA_ARGS}"
    if [ -z ${background} ] && [ -z ${heapdump} ] ; then
        $LAUNCHER $VMPARMS $JVM_OPTS $JAVA_ARGS
    else
        nohup $LAUNCHER $VMPARMS $JVM_OPTS $JAVA_ARGS \
              > ${bench}.out 2> ${bench}.err < /dev/null &
        echo $! > ${bench}.pid
    fi
fi

if [ ! -z ${heapdump} ] ; then
    pid=`cat ${bench}.pid`
    while kill -0 $pid 2> /dev/null ;
    do
        sleep ${frequency}
        date=`(date +"%T")`
        jmap -J-d64 -dump:live,file="${heapdump_dir}/$date.readonly.hprof" $pid
    done
fi

exit $?




#!/bin/sh

while :
do
    case "$1" in
        -sparkey)
            ## Parse sparkey options
            bench=sparkey
            shift
            if [ ! -f "$1" ] ; then
                echo "JAR File is invalid: $1"
                exit -1
            else
                jardir="$1"
                shift
            fi

            for prop in "$@"
            do
                case "$1" in
                    -*)
                        sparkey_props="$sparkey_props $1 $2"
                        shift 2
                        ;;
                    *)
                        ## TODO: Assert, here, that this is a test to call
                        test="$1"
                        shift
                        break
                        ;;
                esac
            done
            ;;
        -local)
            bench=local
            shift
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
#JVM_OPTS="$JVM_OPTS -XX:+PrintGC"
JVM_OPTS="$JVM_OPTS -XX:+PrintGCDetails"
JVM_OPTS="$JVM_OPTS -XX:+PrintGCDateStamps"
#JVM_OPTS="$JVM_OPTS -XX:+PrintHeapAtGC"
JVM_OPTS="$JVM_OPTS -Xloggc:/media/storage2/GCLogs/`date +%H-%M-%S`.${bench}.gclog"
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
if [ "${bench}" = "sparkey" ] ; then
    sparkey_props="$sparkey_props -jvm $LAUNCHER"
    if [ -z ${background} ] ; then
        $LAUNCHER $VMPARMS $JVM_OPTS -jar ${jardir} ${sparkey_props} '-jvmArgs' "$VMPARMS $JVM_OPTS" ${test}
    else
        nohup $LAUNCHER $VMPARMS $JVM_OPTS -jar ${jardir} ${sparkey_props} '-jvmArgs' "$VMPARMS $JVM_OPTS" ${test} > ${bench}.out 2> ${bench}.err < /dev/null &
        echo $! > ${bench}.pid
    fi
elif [ "${bench}" = "local" ] ; then
    JAVA_ARGS="-classpath ./Microbenchmark/ $@"
    echo "JAVA_ARGS to be used ${JAVA_ARGS}"
    $LAUNCHER $VMPARMS $JVM_OPTS $JAVA_ARGS
fi

exit $?




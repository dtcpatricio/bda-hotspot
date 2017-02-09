#!/bin/sh

jardir="$1"
test="$2"

VM_SO_DIR=/media/storage2/hs-base/linux_amd64_compiler2/product/
JAVA_HOME=/media/storage2/jdk8u60/build/linux-x86_64-normal-server-release/jdk/
JDK=${JAVA_HOME}
VMPARMS="-Dsun.java.launcher=gamma -XXaltjvm=$VM_SO_DIR"
LAUNCHER=$JDK/bin/java
if [ ! -x $LAUNCHER ] ; then
    echo "Error: Cannot find the java launcher $LAUNCHER"
    exit 1
fi
export JAVA_HOME


### JVM OPTIONS
## Most are obligatory therefore do not disable them for the sake of the benchmarks.
## The ones with the possibility of being disabled are noted for such.

## GC and Metadata Options
JVM_OPTS="$JVM_OPTS -XX:+UseBDA -XX:BDARatio=1.2"
JVM_OPTS="$JVM_OPTS -XX:BDAKlasses="
JVM_OPTS="$JVM_OPTS -XX:+UseParallelGC"
JVM_OPTS="$JVM_OPTS -XX:+UseParallelOldGC"
JVM_OPTS="$JVM_OPTS -XX:-UseAdaptiveSizePolicy"
JVM_OPTS="$JVM_OPTS -XX:-UsePerfData"
JVM_OPTS="$JVM_OPTS -XX:-UseCompressedOops"
JVM_OPTS="$JVM_OPTS -XX:-UseCompressedClassPointers"
JVM_OPTS="$JVM_OPTS -XX:ParallelGCThreads=4"


## Heap Options
JVM_OPTS="$JVM_OPTS -Xmx8g"
JVM_OPTS="$JVM_OPTS -Xms8g"


## Print Options (these can be commented out)
JVM_OPTS="$JVM_OPTS -XX:+PrintGCDetails"
JVM_OPTS="$JVM_OPTS -XX:+PrintGCDateStamps"

sparkey_props="-i 5 -f 5 -jvm $LAUNCHER"


for i in 20000000 40000000 60000000 80000000 100000000 ;
do
    $LAUNCHER $VMPARMS $JVM_OPTS -jar ${jardir} ${sparkey_props} -p numElements=$i '-jvmArgs' \
              "$VMPARMS $JVM_OPTS -Xloggc:/media/storage2/GCLogs/`date +%H-%M-%S`.base.skey.$i.gclog" \
              ${test} > /media/storage2/EuroPar-Results/base.sparkey.$i.out 2>&1
done

exit $?




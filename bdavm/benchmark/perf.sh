#!/bin/sh

# This script launches perf-stat on a java program (performance counters program)

# The events to be passed onto perf can be selected using the PERF_COUNTER
# variable. This variable is concatenated throughout the script to create
# a comma-separated list of events to be recorded.
# PERF_OPT variable can be used to select multiple perf options.
# The user must provide the Java program and its arguments on the variables,
# JAVA_BIN and JAVA_OPT, respectively.
# If the JAVA_BIN variable is empty, the script assumes the java binary installed
# in the system.
# VM_OPT is available to put in the required VM options to test.

ITERATIONS=3

JAVA_OPT="$1"

# Custom JAVA_HOME
JAVA_HOME=/media/storage2/jdk8u60/build/linux-x86_64-normal-server-release/jdk/
export JAVA_HOME

# The java bin executable
JAVA_BIN=/media/storage2/jdk8u60/build/linux-x86_64-normal-server-release/jdk/bin/java

# VM selection. Certain VMs may not cope with some of the options below
VM="-Dsun.java.launcher=gamma -XXaltjvm=/media/storage2/hs-base/linux_amd64_compiler2/product/"

# Classpath options
# CLASSPATH="${CLASSPATH}"

# GC and Metadata options
VM_OPTS="${VM_OPTS} -XX:+UseParallelGC"
VM_OPTS="${VM_OPTS} -XX:+UseParallelOldGC"
VM_OPTS="${VM_OPTS} -XX:+UseBDA"
VM_OPTS="${VM_OPTS} -XX:BDAKlasses=MyHashMap"
VM_OPTS="${VM_OPTS} -XX:BDARatio=1.2"
VM_OPTS="${VM_OPTS} -XX:-UseCompressedOops"
VM_OPTS="${VM_OPTS} -XX:-UseCompressedClassPointers"
VM_OPTS="${VM_OPTS} -XX:-UsePerfData"
VM_OPTS="${VM_OPTS} -XX:-UseAdaptiveSizePolicy"
VM_OPTS="${VM_OPTS} -XX:-PSChunkLargeArrays"
VM_OPTS="${VM_OPTS} -Xmx8g"
VM_OPTS="${VM_OPTS} -Xms8g"

# This one must be last
VM_OPTS="${VM_OPTS} ${CLASSPATH}"

# Java options (includes the executable or the classpath then the executable)
JAVA_OPT="${JAVA_OPT}"


#PERF_COUNTER="${PERF_COUNTER}L1-dcache-loads,"
#PERF_COUNTER="${PERF_COUNTER}L1-dcache-load-misses,"
#PERF_COUNTER="${PERF_COUNTER}LLC-loads,"
#PERF_COUNTER="${PERF_COUNTER}LLC-load-misses,"
#PERF_COUNTER="${PERF_COUNTER}dTLB-loads,"
#PERF_COUNTER="${PERF_COUNTER}dTLB-load-misses,"
# PERF_COUNTER="${PERF_COUNTER}cache-references,"
# PERF_COUNTER="${PERF_COUNTER}cache-misses,"
PERF_COUNTER="${PERF_COUNTER}page-faults"
# PERF_COUNTER="${PERF_COUNTER}instructions"


# Area for perf options and performance counter events
PERF_OPT="${PERF_OPT} -c"
PERF_OPT="${PERF_OPT} -e ${PERF_COUNTER}"
#PERF_OPT="${PERF_OPT} --repeat=$ITERATIONS"
PERF_OPT="${PERF_OPT} --big-num"
PERF_OPT="${PERF_OPT} -I 250"


CMD="${JAVA_BIN} ${VM} ${VM_OPTS} ${JAVA_OPT}"
echo "Running perf stat with executable:"
echo "${CMD}"

perf stat ${PERF_OPT} ${CMD}

exit $?
         






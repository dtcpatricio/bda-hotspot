#!/bin/sh

# In this script, include all the JVM Options to pass onto the launcher.

## Common Options
JVM_ARGS="${JVM_ARGS} -server"
JVM_ARGS="${JVM_ARGS} -XX:-TieredCompilation"

## GC Options
JVM_ARGS="${JVM_ARGS} -XX:+UseParallelGC"
JVM_ARGS="${JVM_ARGS} -XX:+UseParallelOldGC"
JVM_ARGS="${JVM_ARGS} -XX:-UseCompressedOops"
JVM_ARGS="${JVM_ARGS} -XX:-UseCompressedClassPointers"

## Printing / Logging Options
JVM_ARGS="${JVM_ARGS} -XX:+PrintGC"
JVM_ARGS="${JVM_ARGS} -XX:+PrintGCDetails"
JVM_ARGS="${JVM_ARGS} -XX:+PrintGCApplicationStoppedTime"
JVM_ARGS="${JVM_ARGS} -XX:+PrintGCDateStamps"
JVM_ARGS="${JVM_ARGS} -XX:+PrintHeapAtGC"
JVM_ARGS="${JVM_ARGS} -Xloggc:/tmp/jvm.log"


## BDA Options
##
# Performance / Tuning Options
# JVM_ARGS="${JVM_ARGS} -XX:+UseBDA"
JVM_ARGS="${JVM_ARGS} -XX:BDARatio=5.0"
JVM_ARGS="${JVM_ARGS} -XX:BDAKlasses=MyHashMap"
# JVM_ARGS="${JVM_ARGS} -XX:BDADelegationLevel="
# JVM_ARGS="${JVM_ARGS} -XX:BDACollectionSize="
# JVM_ARGS="${JVM_ARGS} -XX:BDAElementNumberFields="
# JVM_ARGS="${JVM_ARGS} -XX:BDAContainerFraction="
# JVM_ARGS="${JVM_ARGS} -XX:BDAContainerNodeFields="

# Printing Options (some are very verbose)
# JVM_ARGS="${JVM_ARGS} -XX:+TraceBDAClassAssociation"
# JVM_ARGS="${JVM_ARGS} -XX:+PrintEnqueuedContainers"
JVM_ARGS="${JVM_ARGS} -XX:+AlertContainersInOtherSpace"
# JVM_ARGS="${JVM_ARGS} -XX:+PrintBDAContentsAtFullGC"
# JVM_ARGS="${JVM_ARGS} -XX:+PrintBDAContentsAtGC"
# JVM_ARGS="${JVM_ARGS} -XX:+ContainerFragmentationAtFullGC"
JVM_ARGS="${JVM_ARGS} -XX:+ContainerFragmentationAtGC"
# JVM_ARGS="${JVM_ARGS} -XX:+BDAPrintAfterGC"
# JVM_ARGS="${JVM_ARGS} -XX:+BDAPrintAfterMinorGC"
# JVM_ARGS="${JVM_ARGS} -XX:+BDAPrintDescriptive"
# JVM_ARGS="${JVM_ARGS} -XX:+BDAPrintOnlyCollections"
# The level of verbosity for allocations on bda containers
# JVM_RAGS="${JVM_ARGS} -XX:BDAllocationVerboseLevel=0"
## These are only debug flavor flags (are commented out for use only on debug builds)
# JVM_RAGS="${JVM_ARGS} -XX:+BDAPrintAllContainers"
# JVM_RAGS="${JVM_ARGS} -XX:+BDAPrintOldToYoungTasks"


## Size options
JVM_ARGS="${JVM_ARGS} -Xms6g"
JVM_ARGS="${JVM_ARGS} -Xmx6g"
JVM_ARGS="${JVM_ARGS} -XX:MaxNewSize=512m"
JVM_ARGS="${JVM_ARGS} -XX:NewSize=512m"

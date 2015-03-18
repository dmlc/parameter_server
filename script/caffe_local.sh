#!/bin/bash
# set -x
# export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../third_party/lib
if [ $# -lt 3 ]; then
    echo "usage: ./local.sh num_servers num_workers root_dir solver.prototxt [args..]"
    echo "solver.prototxt resides in subdirectories[S0,S1,...,W0,W1,...] of root_dir"
    exit -1;
fi

bin=$(pwd)/build/caffe
num_servers=$1
shift
num_workers=$1
shift
root_dir=$1
shift
solver=$1
shift
arg="-num_servers ${num_servers} -num_workers ${num_workers} $@" #" -app ${dir}/$@"


# killall -q $(basename ${bin})
killall -q ${bin}
sleep 1

silence=">/dev/null 2>/dev/null"


# start the scheduler
Sch="role:SCHEDULER,hostname:'127.0.0.1',port:8001,id:'H'"
${bin} -my_node ${Sch} -scheduler ${Sch} ${arg} >/dev/null 2>/dev/null &

# start servers
for ((i=0; i<${num_servers}; ++i)); do
    port=$((9600 + ${i}))
    id=S${i}
    N="role:SERVER,hostname:'127.0.0.1',port:${port},id:'${id}'"
    # HEAPPROFILE=/tmp/S${i} \
    # CPUPROFILE=/tmp/S${i} \
    cd $root_dir/$id/ && ${bin} -my_node ${N} -scheduler ${Sch} --solver=$solver ${arg} >$root_dir/$id/stdout.txt 2>&1 &
done

# start workers
for ((i=0; i<${num_workers}; ++i)); do
    port=$((9500 + ${i}))
    id=W${i}
    N="role:WORKER,hostname:'127.0.0.1',port:${port},id:'${id}'"
    # HEAPPROFILE=/tmp/W${i} \
    # CPUPROFILE=/tmp/W${i} \
    cd $root_dir/$id/ && ${bin} -my_node ${N} -scheduler ${Sch} --solver=$solver ${arg} >$root_dir/$id/stdout.txt 2>&1 &
done

wait

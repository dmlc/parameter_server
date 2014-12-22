#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../third_party/lib
if [ $# -lt 3 ]; then
    echo "usage: ./local.sh num_servers num_workers app_conf [args]"
    exit -1;
fi
dir=`dirname "$0"`
cd ${dir}

killall -q ps

num_servers=$1
shift
num_workers=$1
shift
bin="../bin/ps"
arg="-num_servers ${num_servers} -num_workers ${num_workers} -app ${dir}/$@"

mkdir -p ../output
FLAGS_logtostderr=1

# start the scheduler
Sch="role:SCHEDULER,hostname:'127.0.0.1',port:8000,id:'H'"
${bin} -my_node ${Sch} -scheduler ${Sch} ${arg} &

# start servers
for ((i=0; i<${num_servers}; ++i)); do
    port=$((8100 + ${i}))
    N="role:SERVER,hostname:'127.0.0.1',port:${port},id:'S${i}'"
    # CPUPROFILE=/tmp/S${i} \
    ${bin} -my_node ${N} -scheduler ${Sch} ${arg} &
done

# start workers
for ((i=0; i<${num_workers}; ++i)); do
    port=$((8200 + ${i}))
    N="role:WORKER,hostname:'127.0.0.1',port:${port},id:'W${i}'"
    # CPUPROFILE=/tmp/W${i} \
    ${bin} -my_node ${N} -scheduler ${Sch} ${arg} &
done

wait

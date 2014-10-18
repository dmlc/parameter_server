#!/bin/bash

dir=`dirname "$0"`
cd ${dir}

killall -q ps

scheduler="role:SCHEDULER,hostname:'127.0.0.1',port:8000,id:'H'"
W0="role:WORKER,hostname:'127.0.0.1',port:8001,id:'W0'"
W1="role:WORKER,hostname:'127.0.0.1',port:8002,id:'W1'"
W2="role:WORKER,hostname:'127.0.0.1',port:8003,id:'W2'"
S0="role:SERVER,hostname:'127.0.0.1',port:8010,id:'S0'"
S1="role:SERVER,hostname:'127.0.0.1',port:8011,id:'S1'"
arg="-num_servers 1 -num_workers 1 -num_threads 2 -app ../config/online_rcv1_l1lr.conf"

mkdir -p ../output
FLAGS_logtostderr=1

bin="../bin/ps"

${bin} ${arg} -scheduler ${scheduler} -my_node ${scheduler} &
${bin} ${arg} -scheduler ${scheduler} -my_node ${W0} &
${bin} ${arg} -scheduler ${scheduler} -my_node ${S0} &

# ${bin} ${arg} -scheduler ${scheduler} -my_node ${W1} &
# ${bin} ${arg} -scheduler ${scheduler} -my_node ${W2} &
# ${bin} ${arg} -scheduler ${scheduler} -my_node ${S1} &

wait

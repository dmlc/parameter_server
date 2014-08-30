#!/bin/bash

killall ps
scheduler="role:SCHEDULER,hostname:'127.0.0.1',port:8000,id:'H'"
W0="role:WORKER,hostname:'127.0.0.1',port:8001,id:'W0'"
W1="role:WORKER,hostname:'127.0.0.1',port:8002,id:'W1'"
S0="role:SERVER,hostname:'127.0.0.1',port:8010,id:'S0'"
S1="role:SERVER,hostname:'127.0.0.1',port:8011,id:'S1'"
# arg="-num_servers 2 -num_workers 2 -num_threads 4 -app ../config/rcv1_l1lr.config"
arg="-num_servers 2 -num_workers 2 -num_threads 4 -app ../config/rcv1_l1lr.config"

mkdir -p ../output
FLAGS_logtostderr=1
bin="../bin/ps"
${bin} ${arg} -scheduler ${scheduler} -my_node ${scheduler} &
${bin} ${arg} -scheduler ${scheduler} -my_node ${W0} &
# lldb -- ${bin} ${arg} -scheduler ${scheduler} -my_node ${S0}
${bin} ${arg} -scheduler ${scheduler} -my_node ${S0} &

${bin} ${arg} -scheduler ${scheduler} -my_node ${W1} &
${bin} ${arg} -scheduler ${scheduler} -my_node ${S1} &

wait

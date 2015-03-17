#!/bin/bash

BASEDIR=$(dirname $0)/../../..

export LD_LIBRARY_PATH=${BASEDIR}/../minerva/deps/lib:${BASEDIR}/third_party/lib:${LD_LIBRARY_PATH}
export PYTHONPATH=${BASEDIR}/../minerva/release/owl:${BASEDIR}/../minerva/owl:$PYTHONPATH
export PYTHONPATH=${BASEDIR}/src/app/python/:$PYTHONPATH

scheduler="role:SCHEDULER,hostname:'127.0.0.1',port:8000,id:'H'"
W0="role:WORKER,hostname:'127.0.0.1',port:8001,id:'W0'"
W1="role:WORKER,hostname:'127.0.0.1',port:8002,id:'W1'"
S0="role:SERVER,hostname:'127.0.0.1',port:8010,id:'S0'"
arg="-num_servers 1 -num_workers 2 -num_threads 1"
bin=$BASEDIR/build/ps_python
script="$1"
shift

for PID in $(ps aux | grep build/ps_python | grep -v grep | awk '{ print $2; }'); do
  kill -9 $PID
done

$bin $arg -scheduler $scheduler -my_node $scheduler &
$bin $arg -scheduler $scheduler -my_node $S0 -script "$script" -- "$@" &
$bin $arg -scheduler $scheduler -my_node $W0 -script "$script" -- "$@" &
$bin $arg -scheduler $scheduler -my_node $W1 -script "$script" -- "$@"


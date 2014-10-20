#!/bin/bash
if [ $# -lt 3 ]; then
    echo "usage: ./local.sh num_servers num_workers app_conf [args]"
    exit -1;
fi
dir=`dirname "$0"`

killall -q ps

num_servers=$1
shift
num_workers=$1
shift
bin=${dir}/../bin/ps
arg="-num_servers ${num_servers} -num_workers ${num_workers} -app $@"
Sch="role:SCHEDULER,hostname:'127.0.0.1',port:8000,id:'H'"

mkdir -p ../output
FLAGS_logtostderr=1

# start instances
for ((my_rank=0; i<${num_servers}+${num_workers}; ++my_rank)); do
    ${bin} \
    ${Sch} \
    ${arg} \
    -num_threads 4 \
    -my_rank ${my_rank} \
    -report_interval 0 \
    -verbose 0 \
    -log_to_file 0 \
    -log_instant 0 \
    -load_limit 0 \
    -line_limit 0 \
    -print_van 0 \
    -shuffle_fea_id 0 \
    -parallel_match 0 \
done

wait

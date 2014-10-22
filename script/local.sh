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
Sch="role:SCHEDULER,hostname:'127.0.0.1',port:2060,id:'H'"
arg="-num_servers ${num_servers} -num_workers ${num_workers} -scheduler ${Sch} -app ${dir}/$@"

mkdir -p ../output
FLAGS_logtostderr=1

# start instances
for ((my_rank=0; my_rank<=${num_servers}+${num_workers}; ++my_rank)); do
    ${bin} \
    ${arg} \
    -num_threads 4 \
    -my_rank ${my_rank} \
    -report_interval 0 \
    -load_limit 0 \
    -line_limit 0 \
    -noverbose \
    -nolog_to_file \
    -nolog_instant \
    -noprint_van \
    -noshuffle_fea_id \
    -noparallel_match \
    &
done

wait

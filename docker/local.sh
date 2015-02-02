#!/bin/bash

if [ $# -lt 5 ]; then
    echo "usage: $0 num_servers num_workers app_conf_file data_dir model_dir [args...]"
    exit -1;
fi

ip=`/sbin/ifconfig docker0 | grep inet | grep -v inet6 | awk '{print $2}' | sed -e 's/[a-z]*://'`
if [ -z ${ip} ]; then
    echo "failed to get the ip address for docker"
    exit -1
fi

num_servers=$1
shift

num_workers=$1
shift

app=$1
if [[ "$app" != /* ]]; then
    app=`pwd`/$app
fi
shift

data=$1
if [[ "$data" != /* ]]; then
    data=`pwd`/$data
fi
shift

model=$1
if [[ "$model" != /* ]]; then
    model=`pwd`/$model
fi
shift

port=8000
bin="muli/parameter-server /build/ps"
# bin_v="-v /home/muli/work/ps/build:/build"
app_v="-v $app:/app.conf"
data_v="-v $data:/data -v $model:/model"
mount="$bin_v $app_v $data_v"

arg="-app_file /app.conf -num_servers $num_servers -num_workers $num_workers $@"

sch="role:SCHEDULER,hostname:'$ip',port:$port,id:'H'"
for (( i = 0; i < ${num_servers} + ${num_workers} + 1; ++i )); do
    myport=$(($port + ${i}))
    if (( $i == 0)); then
        node=$sch
    elif (( $i <= ${num_servers} )); then
        node="role:SERVER,hostname:'$ip',port:${myport},id:'S${i}'"
    else
        node="role:WORKER,hostname:'$ip',port:${myport},id:'W${i}'"
    fi
    docker run --rm -p $myport:$myport --name n${i} $mount $bin \
        -my_node ${node} -scheduler ${sch} ${arg} &
done

wait

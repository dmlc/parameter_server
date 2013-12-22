#!/bin/bash
BIN=parse

set -x
set -u
killall $BIN

num_client=2
num_server=2
GLOG_logtostderr=true
FLAG="--num_server ${num_server} --num_client ${num_client}"

for (( id=0; id<${num_server}; id++ ))
do
  ./$BIN --my_type server --my_rank ${id} ${FLAG} &> server.${id}.log &
done

for (( id=0; id<${num_client}; id++ ))
do
  ./$BIN --my_type client --my_rank ${id} ${FLAG} &> client.${id}.log &
done

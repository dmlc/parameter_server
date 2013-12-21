#!/bin/bash
ulimit -c unlimited
BIN=vectors_test

#set -x
set -u
killall $BIN

num_client=2
num_server=2
GLOG_logtostderr=true
for (( id=0; id<${num_server}; id++ ))
do
  ./$BIN --my_type server --my_rank ${id} --num_server ${num_server} --num_client ${num_client} &> server.${id}.log &
done

# sleep 1

for (( id=0; id<${num_client}; id++ ))
do
  ./$BIN --my_type client --my_rank ${id} --num_server ${num_server} --num_client ${num_client} &> client.${id}.log &
done

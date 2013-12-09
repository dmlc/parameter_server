#!/bin/bash

set -u
killall -9 fault_tolerance_press
test_ft=false
time_to_crash=2
num_client=2
num_server=2
global_feature_num=100
local_feature_num=100

c_addr=tcp://localhost:7000,tcp://localhost:6010,tcp://localhost:6020,tcp://localhost:6022,tcp://localhost:6024,tcp://localhost:6026,tcp://localhost:6028
s_addr=tcp://localhost:8000,tcp://localhost:8010,tcp://localhost:8020,tcp://localhost:8022,tcp://localhost:8024,tcp://localhost:8026,tcp://localhost:8028

flags=--enable_fault_tolerance
for (( id=0; id<${num_server}; id++ ))
do
  ../fault_tolerance_press --my_type server --my_rank ${id} --num_server ${num_server} --num_client ${num_client} --server_address ${s_addr} --client_address ${c_addr} --global_feature_num ${global_feature_num} --local_feature_num ${local_feature_num} ${flags} &> ft.log.server.${id} &

  crash_pid=$! 
done

sleep 1

for (( id=0; id<${num_client}; id++ ))
do
  ../fault_tolerance_press --my_type client --my_rank ${id} --num_server ${num_server} --num_client ${num_client} --server_address ${s_addr} --client_address ${c_addr} --global_feature_num ${global_feature_num} --local_feature_num ${local_feature_num} ${flags} &> ft.log.client.${id} &
done

if $test_ft; then
  echo "Process $crash_pid is going to be crashed in $time_to_crash seconds..."
  sleep $time_to_crash
  kill -9 $crash_pid
fi

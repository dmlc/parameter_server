#!/bin/bash

set -u
killall -9 fault_tolerance_press
test_ft=false
time_to_crash=2
num_client=3
num_server=3
global_feature_num=100
local_feature_num=100

c_addr=tcp://localhost:7000,tcp://localhost:6010,tcp://localhost:6020,tcp://localhost:6022,tcp://localhost:6024,tcp://localhost:6026,tcp://localhost:6028
s_addr=tcp://localhost:8000,tcp://localhost:8010,tcp://localhost:8020,tcp://localhost:8022,tcp://localhost:8024,tcp://localhost:8026,tcp://localhost:8028

flags=--enable_fault_tolerance
for (( id=0; id<${num_server}; id++ ))
do
# if [ ${id} -eq 2 ]; then
# flags="${flags} --is_backup_process"
# fi
  ../fault_tolerance_press --my_type server --my_rank ${id} --num_server ${num_server} --num_client ${num_client} --server_address ${s_addr} --client_address ${c_addr} --global_feature_num ${global_feature_num} --local_feature_num ${local_feature_num} ${flags} &> ft.log.server.${id} &

  crash_pid=$! 
done

# flags=--enable_fault_tolerance
echo "All servers have been started"

sleep 1

for (( id=0; id<${num_client}; id++ ))
do
  ../fault_tolerance_press --my_type client --my_rank ${id} --num_server ${num_server} --num_client ${num_client} --server_address ${s_addr} --client_address ${c_addr} --global_feature_num ${global_feature_num} --local_feature_num ${local_feature_num} ${flags} &> ft.log.client.${id} &
done

echo "All clients have been started."

if $test_ft; then
  echo "Process $crash_pid is going to be crashed in $time_to_crash seconds..."
  sleep $time_to_crash
  echo $crash_pid
  kill -9 $crash_pid
  echo "Process $crash_pid has been killed"
fi

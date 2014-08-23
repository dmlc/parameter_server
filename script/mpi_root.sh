#!/bin/bash

source ../config/mpi.conf

my_ip=`ifconfig ${network_interface} | grep inet | grep -v inet6 | awk '{print $2}'`
if [ -z ${my_ip} ]; then
    echo "failed to get the ip address"
    exit -1
fi

root_node="role:SCHEDULER,hostname:'${my_ip}',port:${port},id:'H'"
np=$((${num_workers} + ${num_servers} + 1))

if [ -z ${hostfile} ]; then
    mpirun -np ${np} ./mpi_node.sh ${root_node}
else
    mpirun -np ${np} -hostfile ${hostfile} ./mpi_node.sh ${root_node}
fi

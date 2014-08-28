#!/bin/bash

if [ $# -ne 1 ]; then
    echo "usage: ./self scheduler_node"
    exit -1;
fi

# support mpich and openmpi
# try mpirun -n 1 env to get all available environment

if [ -n ${PMI_RANK} ]; then
    my_rank=${PMI_RANK}
elif [ -n ${OMPI_COMM_WORLD_RANK} ]; then
    my_rank=${OMPI_COMM_WORLD_RANK}
else
    echo "failed to get my rank id"
    exit -1
fi

if [ -n ${PMI_SIZE} ]; then
    rank_size=${PMI_SIZE}
elif [ -n ${OMPI_COMM_WORLD_SIZE}]; then
    rank_size=${OMPI_COMM_WORLD_SIZE}
else
    echo "failed to get the rank size"
    exit -1
fi

source ../config/mpi.conf


if (( ${rank_size} < ${num_workers} + ${num_servers} + 1 )); then
    echo "too small rank size ${rank_size}"
    exit -1
fi

my_ip=`ifconfig ${network_interface} | grep inet | grep -v inet6 | awk '{print $2}'`
if [ -z ${my_ip} ]; then
    echo "failed to get the ip address"
    exit -1
fi

my_port=$(( ${port} + ${my_rank} ))

# rank 0 : scheduler
# rank 1 to num_servers : server nodes
# rank num_servers + 1 to num_servers + num_workers : worker nodes
# rest: unused (backup) nodes

if (( ${my_rank} == 0 )); then
    my_node=${1}
elif (( ${my_rank} < ${num_servers} )); then
    my_node="role:SERVER,hostname:'${my_ip}',port:${my_port},id:'S${my_rank}'"
elif (( ${my_rank} < ${num_servers} + ${num_workers} )); then
    my_node="role:WORKER,hostname:'${my_ip}',port:${my_port},id:'C${my_rank}'"
else
    my_node="role:UNUSED,hostname:'${my_ip}',port:${my_port},id:'U${my_rank}'"
fi

echo $my_node

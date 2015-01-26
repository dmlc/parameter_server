#!/bin/bash
# set -x

if [ $# -ne 1 ]; then
    echo "usage: ./self mpi.conf"
    exit -1;
fi

conf=${1}
source ${conf}

# mpirun=${dir}/mpirun

dir=`dirname "$0"`
root_node=`${dir}/get_root_node.sh ${network_interface} ${network_port}`
np=$((${num_workers} + ${num_servers} + 1))

if [ ! -z ${hostfile} ]; then
    hostfile="-hostfile ${hostfile}"
fi

# mpirun ${hostfile} killall -q ps
# mpirun ${hostfile} md5sum ../bin/ps

mpirun ${hostfile} -np ${np} ${dir}/mpi_node.sh ${root_node} ${conf}

#!/bin/bash
# set -x

if [ $# -ne 1 ]; then
    echo "usage: ./self mpi.conf"
    exit -1;
fi

dir=`dirname "$0"`
conf=${dir}/${1}
# mpirun=${dir}/mpirun

source ${conf}

root_node=`${dir}/get_root_node.sh ${network_interface} ${network_port}`
np=$((${num_workers} + ${num_servers} + 1))

if [ ! -z ${hostfile} ]; then
    hf="-hostfile ${dir}/${hostfile}"
fi

# mpirun ${hf} killall -q ps
# mpirun ${hf} md5sum ../bin/ps

mpirun ${hf} -np ${np} ${dir}/mpi_node.sh ${root_node} ${conf} ${dir}

#!/bin/bash
# set -x

if [ $# -ne 1 ]; then
    echo "usage: ./self mpi.conf"
    exit -1;
fi

dir=`pwd`
conf=${dir}/${1}
cd `dirname "$0"`

source ${conf}

my_ip=`ifconfig ${network_interface} | grep inet | grep -v inet6 | awk '{print $2}' | sed -e 's/[a-z]*:/''/'`
if [ -z ${my_ip} ]; then
    echo "failed to get the ip address"
    exit -1
fi

root_node="role:SCHEDULER,hostname:'${my_ip}',port:${network_port},id:'H'"
np=$((${num_workers} + ${num_servers} + 1))

if [ ! -z ${hostfile} ]; then
    hf="-hostfile ${hostfile}"
fi

# mpirun ${hf} killall -q ps
# mpirun ${hf} md5sum ../bin/ps

mpirun ${hf} -np ${np} ./mpi_node.sh ${root_node} ${conf}

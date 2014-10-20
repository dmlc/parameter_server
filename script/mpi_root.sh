#!/bin/bash
# set -x

if [ $# -ne 1 ]; then
    echo "usage: ./self mpi.conf"
    exit -1;
fi

dir=`dirname "$0"`
conf=${dir}/${1}
mpirun=${dir}/mpirun

source ${conf}

my_ip=`/sbin/ifconfig ${scheduler_network_interface} | grep inet | grep -v inet6 | awk '{print $2}' | sed -e 's/[a-z]*:/''/'`
if [ -z ${my_ip} ]; then
    echo "failed to get the ip address"
    exit -1
fi

root_node="role:SCHEDULER,hostname:'${my_ip}',port:${scheduler_network_port},id:'H'"
np=$((${num_workers} + ${num_servers} + 1))

if [ ! -z ${hostfile} ]; then
    hf="-hostfile ${dir}/${hostfile}"
fi

# mpirun ${hf} killall -q ps
# mpirun ${hf} md5sum ../bin/ps

${mpirun} ${hf} -np ${np} ${dir}/mpi_node.sh ${root_node} ${conf} ${dir}

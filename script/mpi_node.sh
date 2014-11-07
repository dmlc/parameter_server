#!/bin/bash
# set -x
if [ $# -ne 3 ]; then
    echo "usage: ./self scheduler_node mpi.conf dir_of_mpi_node_sh"
    exit -1;
fi

# support mpich and openmpi
# try mpirun -n 1 env to get all available environment
if [ ! -z ${PMI_RANK} ]; then
    my_rank=${PMI_RANK}
elif [ ! -z ${OMPI_COMM_WORLD_RANK} ]; then
    my_rank=${OMPI_COMM_WORLD_RANK}
else
    echo "failed to get my rank id"
    exit -1
fi

if [ ! -z ${PMI_SIZE} ]; then
    rank_size=${PMI_SIZE}
elif [ ! -z ${OMPI_COMM_WORLD_SIZE} ]; then
    rank_size=${OMPI_COMM_WORLD_SIZE}
else
    echo "failed to get the rank size"
    exit -1
fi
# echo "$my_rank $rank_size"

source ${2}

if (( ${rank_size} < ${num_workers} + ${num_servers} + 1 )); then
    echo "too small rank size ${rank_size}"
    exit -1
fi

mkdir -p ${3}/../output
${3}/ps \
    -num_servers ${num_servers} \
    -num_workers ${num_workers} \
    -num_threads ${num_threads} \
    -scheduler ${1} \
    -my_rank ${my_rank} \
    -app ${3}/${app_conf} \
    -report_interval ${report_interval} \
    ${verbose} \
    ${log_to_file} \
    ${log_instant} \
    ${print_van} \
    ${shuffle_fea_id} \
    || { echo "rank:${my_rank} launch failed"; exit -1; }

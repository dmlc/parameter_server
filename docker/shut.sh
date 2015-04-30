#!/bin/bash
if [[ $# -lt 1 ]]; then
    echo "usage: $0 n-instances"
    exit -1
fi

n=$1
docker-machine stop swarm-master > /dev/null 2>&1 &
for (( i = 0; i < n-1; i++ )); do
	docker-machine stop swarm-node-$i > /dev/null 2>&1 &
done
wait
docker-machine rm -f swarm-master &
for (( i = 0; i < n-1; i++ )); do
	docker-machine rm -f swarm-node-$i &
done
wait

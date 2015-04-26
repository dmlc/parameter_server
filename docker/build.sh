#!/bin/bash

if [[ $# -lt 2 ]]; then
    echo "usage: $0 manager image"
    exit -1
fi

if [[ ! -e ../Dockerfile ]]; then
	echo "cannot find Dockerfile! this command must be excuted under docker/"
	exit -1
fi

manager=$1
shift

image=$1
shift

eval "`docker-machine env $manager`"
docker build -t $image ..
docker push $image
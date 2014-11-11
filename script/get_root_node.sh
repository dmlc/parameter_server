#!/bin/bash
if [ $# -ne 2 ]; then
    echo "usage: ./self interface port"
    exit -1;
fi

ip=`/sbin/ifconfig ${1} | grep inet | grep -v inet6 | awk '{print $2}' | sed -e 's/[a-z]*:/''/'`
if [ -z ${ip} ]; then
    echo "failed to get the ip address"
    exit -1
fi

echo "role:SCHEDULER,hostname:'${ip}',port:${2},id:'H'"

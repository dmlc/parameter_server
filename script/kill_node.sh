#!/bin/bash
if [ $# -ne 1 ]; then
    echo "usage: $0 node_id"
    exit -1;
fi

out=/tmp/aux
ps aux >$out
pid=`grep \'$1\'.*scheduler $out | awk '{print $2}'`
if [ ! -z "$pid" ]; then
    kill -9 $pid
fi
rm -f $out

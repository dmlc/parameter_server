#!/bin/bash
# set -x
# export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../third_party/lib

if [ $# -lt 1 ]; then
    echo "usage: ./caffe_kill_lan.sh conf_path"
    echo "solver.prototxt resides in subdirectories[S0,S1,...,W0,W1,...] of root_dir"
    exit -1;
fi

conf=$1
tmp=$( mktemp )
grep -v ^# $conf > $tmp
conf=$tmp

echo "kill local"

killall caffe

echo "kill servers"
# kill servers
grep -E "WORKER|SERVER" $conf | awk '{print $2;}' | sort | uniq | awk  -v q="'" '
{
    cmd="ssh immars@" $0 " \"killall caffe\" ";
    print cmd;
    system(cmd);
}
'

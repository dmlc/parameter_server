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
app=$( grep PS_PATH $conf | awk -F'/' '{print $NF;}' )
echo "app: $app"
echo "kill local"

killall caffe || killall $app

echo "kill servers"
# kill servers
grep -E "WORKER|SERVER" $conf | awk '{print $2;}' | sort | uniq | awk  -v q="'" -v app="$app" '
{
    cmd="ssh immars@" $0 " \"killall caffe || killall " app " \" ";
    print cmd;
    system(cmd);
}
'

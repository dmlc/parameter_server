#!/bin/bash
# set -x
# export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../third_party/lib
if [ $# -lt 1 ]; then
    echo "usage: ./local.sh num_servers num_workers root_dir solver.prototxt [args..]"
    echo "solver.prototxt resides in subdirectories[S0,S1,...,W0,W1,...] of root_dir"
    exit -1;
fi

conf=$1
tmp=$( mktemp )
grep -v ^# $conf > $tmp
conf=$tmp

bin=$( grep PS_PATH $conf | awk '{print $2;}' )
sch_ip=$( grep SCHEDULER $conf | awk '{print $2;}' )
num_servers=$( grep SERVER $conf | wc -l )
num_workers=$( grep WORKER $conf | wc -l )
pullstep=$( grep PULL $conf | awk '{print $2;}' )
pushstep=$( grep PUSH $conf | awk '{print $2;}' )

arg="-num_servers ${num_servers} -num_workers ${num_workers} $@" #" -app ${dir}/$@"

echo "$bin $conf $sch_ip $num_servers $num_workers"
killall -q caffe

silence=">/dev/null 2>/dev/null"


# start the scheduler
Sch="role:SCHEDULER,hostname:'$sch_ip',port:8001,id:'H'"
${bin} -my_node ${Sch} -scheduler ${Sch} ${arg} >/dev/null 2>/dev/null &

# start servers
grep SERVER $conf | awk -v bin=$bin -v sch=$Sch -v nums=$num_servers -v numw=$num_workers -vpullstep=$pullstep -vpushstep=$pushstep -v q="'" '
BEGIN{port=9600;id=0;}
{
    ip=$2;wd=$3;solver=$4;gpu=$5;snapshot=$6;
    if(""!=snapshot){
        snapshot= " --snapshot=" snapshot;
    }
    cmd="ssh -f -n immars@" ip " \"source /etc/profile && cd " wd " && nohup " bin " -num_servers " nums " -num_workers " numw " -my_node \\\"role:SERVER,hostname:" q ip q ",port:" port ",id:" q "S" id q "\\\" -scheduler \\\"" sch "\\\" --solver=" solver " --pullstep=" pullstep " --pushstep=" pushstep " --gpu=" gpu " " snapshot " >" wd "/stdout.txt 2>&1 < /dev/null &\" ";
    print cmd;
    system(cmd);
    port=port+1;id=id+1;
}
'

grep WORKER $conf | awk -v bin=$bin -v sch=$Sch -v nums=$num_servers -v numw=$num_workers -vpullstep=$pullstep -vpushstep=$pushstep -v q="'" '
BEGIN{port=9500;id=0;}
{
    ip=$2;wd=$3;solver=$4;gpu=$5;
    cmd="ssh -f -n immars@" ip " \"source /etc/profile && cd " wd " && nohup " bin " -num_servers " nums " -num_workers " numw " -my_node \\\"role:WORKER,hostname:" q ip q ",port:" port ",id:" q "W" id q "\\\" -scheduler \\\"" sch "\\\" --solver=" solver " --pullstep=" pullstep " --pushstep=" pushstep " --gpu=" gpu " >" wd "/stdout.txt 2>&1 < /dev/null &\" ";
    print cmd;
    system(cmd);
    port=port+1;id=id+1;
}
'


for ((i=0; i<${num_servers}; ++i)); do
    port=$((9600 + ${i}))
    id=S${i}
    N="role:SERVER,hostname:'127.0.0.1',port:${port},id:'${id}'"
    # HEAPPROFILE=/tmp/S${i} \
    # CPUPROFILE=/tmp/S${i} \
#    cd $root_dir/$id/ && ${bin} -my_node ${N} -scheduler ${Sch} --solver=$solver ${arg} >$root_dir/$id/stdout.txt 2>&1 &
done

# start workers
for ((i=0; i<${num_workers}; ++i)); do
    port=$((9500 + ${i}))
    id=W${i}
    N="role:WORKER,hostname:'127.0.0.1',port:${port},id:'${id}'"
    # HEAPPROFILE=/tmp/W${i} \
    # CPUPROFILE=/tmp/W${i} \
#    cd $root_dir/$id/ && ${bin} -my_node ${N} -scheduler ${Sch} --solver=$solver ${arg} >$root_dir/$id/stdout.txt 2>&1 &
done


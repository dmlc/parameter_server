#!/bin/bash
serverip=`hostname -i`
kube_server="./kube-server-up.sh"
kube_client="./kube-client-up.sh" 
cat ../host/minions_file | ${kube_server}
if [ $# -gt 0 ] 
then cat ../host/minions_file | ${kube_client} ${serverip} $1
else cat ../host/minions_file | ${kube_client} ${serverip}
fi

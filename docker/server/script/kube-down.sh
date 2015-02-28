#!/bin/bash
kube_server="./kube-server-down.sh"
kube_client="./kube-client-down.sh"
${kube_server}
if [ $# -gt 0 ]
then cat ../host/minions_file | ${kube_client} $1
else cat ../host/minions_file | ${kube_client}
fi


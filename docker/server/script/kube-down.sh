#!/bin/bash
kube_server="./kube-server-down.sh"
kube_client="./kube-client-down.sh"
${kube_server}
cat ../host/minions_file | ${kube_client}

#!/bin/bash
while read line
do
	kubelet="docker run -d -p 10250:10250 -v /var/run/docker.sock:/var/run/docker.sock -w /home/kubernetes --name kubelet qicongc/kubeserver ./kubelet --address=0.0.0.0 --hostname_override=${line} --etcd_servers=http://$1:4001 --logtostderr=true"
	kubecfg="export KUBERNETES_MASTER=$1:8080"
	if [ $# -gt 1 ]; then
		ssh -i $2 ${line} ${kubelet} < /dev/null
		ssh -i $2 ${line} "echo ${kubecfg} > ~/.bashrc" < /dev/null
		ssh -i $2 ${line} "source ~/.bashrc" < /dev/null
		ssh -i $2 ${line} "mkdir -p /tmp/docker/host" < /dev/null
		ssh -i $2 ${line} "echo ${line} > /tmp/docker/host/host" < /dev/null
	else
		ssh ${line} ${kubelet} < /dev/null
                ssh ${line} "echo ${kubecfg} > ~/.bashrc" < /dev/null
                ssh ${line} "source ~/.bashrc" < /dev/null
                ssh ${line} "mkdir -p /tmp/docker/host" < /dev/null
                ssh ${line} "echo ${line} > /tmp/docker/host/host" < /dev/null
	fi
done

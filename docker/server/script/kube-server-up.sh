#!/bin/bash
hostip=`hostname -i`
#start etcd
etcd="docker run -d -p 4001:4001 --name etcd -v /etc/ssl/certs/:/etc/ssl/certs/  quay.io/coreos/etcd:v0.4.6"
#start kube-apiserver
kube_apiserver="docker run -d -p 8080:8080  -w /home/kubernetes --name apiserver qicongc/kubeserver ./kube-apiserver  --address=0.0.0.0 --etcd_servers=http://${hostip}:4001 --logtostderr=true --portal_net=11.1.1.0/24"
#start kube-controller-manager
kube_controller_manager="docker run -d -w /home/kubernetes --name controller-manager qicongc/kubeserver ./kube-controller-manager --master=${hostip}:8080 --logtostderr=true --machines="
i=0
while read line
do
	i=$((i+1))
    	if [ ${i} -gt 1 ]; then
		kube_controller_manager="${kube_controller_manager},"	
    	fi
	kube_controller_manager="${kube_controller_manager}${line}"
done 
#start kube-scheduler
kube_scheduler="docker run -d -w /home/kubernetes --name scheduler qicongc/kubeserver ./kube-scheduler --master=${hostip}:8080  --logtostderr=true"
#configure kubecfg
kubecfg="export KUBERNETES_MASTER=${hostip}:8080"

${etcd}
${kube_apiserver}
${kube_controller_manager}
${kube_scheduler}
echo ${kubecfg} > ~/.bashrc
source ~/.bashrc 

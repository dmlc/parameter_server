#!/bin/bash
if [[ $# -lt 10 ]]; then
    echo "usage: $0 n-instances amazonec2-instance-type spot|regular price-to-bid amazonec2-access-key amazonec2-secret-key amazonec2-region amazonec2-vpc-id amazonec2-security-group amazonec2-zone"
    exit -1
fi

if [[ ! `which docker-machine` ]]; then
	rm -rf machine
	git clone https://github.com/docker/machine
	cd machine
	script/build -osarch="darwin/amd64"
	mv docker-machine_darwin* /usr/local/bin/docker-machine
	cd ..
	rm -rf machine
fi

n=$1
shift

amazonec2_instance_type=$1
shift

if [[ $1 = 'spot' ]]; then
	shift
	amazonec2_request_spot_instance="--amazonec2-request-spot-instance --amazonec2-spot-price $1"
	shift
elif [[ $1 = 'regular' ]]; then
	shift
	shift
else
	echo "Currently only support regular and spot, but get $1."
	exit -1
fi



amazonec2_access_key=$1
shift

amazonec2_secret_key=$1
shift

amazonec2_region=$1
shift

amazonec2_vpc_id=$1
shift

amazonec2_security_group=$1
shift

amazonec2_zone=$1
shift

discovery="token://`docker run swarm create`"

docker-machine create -d amazonec2 --amazonec2-access-key $amazonec2_access_key --amazonec2-secret-key $amazonec2_secret_key --amazonec2-region $amazonec2_region --amazonec2-vpc-id $amazonec2_vpc_id --amazonec2-security-group $amazonec2_security_group --amazonec2-zone $amazonec2_zone --amazonec2-instance-type $amazonec2_instance_type $amazonec2_request_spot_instance --swarm --swarm-master --swarm-discovery $discovery swarm-master &
for (( i = 0; i < n-1; i++ )); do
	docker-machine create -d amazonec2 --amazonec2-access-key $amazonec2_access_key --amazonec2-secret-key $amazonec2_secret_key --amazonec2-region $amazonec2_region --amazonec2-vpc-id $amazonec2_vpc_id --amazonec2-security-group $amazonec2_security_group --amazonec2-zone $amazonec2_zone --amazonec2-instance-type $amazonec2_instance_type $amazonec2_request_spot_instance --swarm --swarm-discovery $discovery swarm-node-$i &
done
wait



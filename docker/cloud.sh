#!/bin/sh

if [[ $# -lt 6 ]]; then
    echo "usage: $0 cloud_provider manager image num_servers num_workers app_conf_file [args...]"
    exit -1
fi

cloud_provider=$1
shift

manager=$1
shift

mount="-v /tmp/parameter_server/data/cache:/home/parameter_server/data/cache"
case $cloud_provider in
    amazonec2) 
        mount="$mount -v /var/log/cloud-init.log:/var/log/cloud-init.log"
        scheduler_host=`docker-machine ssh $manager cat /var/log/cloud-init.log | awk '/update hostname/ {print $10}'`
    ;;
    *) 
        echo "Currently only support amazonec2!"
        exit -1 
    ;;
esac


image=$1
shift

num_servers=$1
shift

num_workers=$1
shift

app_file=$1
shift

args=$@

# stop all running containers
echo "cleaning previous apps ..."
eval "`docker-machine env --swarm $manager`"
clean_list=`docker ps -q`
docker stop $clean_list > /dev/null 2>&1
echo "update parameter server image cluster wide ..."
docker pull $image


# launch scheduler
echo "launching scheduler ..."
eval "`docker-machine env $manager`"
scheduler_port=8000
env="\
    -e cloud_provider=$cloud_provider \
    -e scheduler_host=$scheduler_host \
    -e scheduler_port=$scheduler_port \
    -e my_role=SCHEDULER \
    -e my_host=$scheduler_host \
    -e my_port=$scheduler_port \
    -e my_id=H \
    -e num_servers=$num_servers \
    -e num_workers=$num_workers \
    -e app_file=$app_file \
    "
docker rm n0 > /dev/null 2>&1
docker run -d -p $scheduler_port:$scheduler_port $env -e "args=$args" --name n0 $image

#launch servers and workers
echo "launching workers and servers ..."
eval "`docker-machine env --swarm $manager`"
for (( i = 1; i < $num_servers + $num_workers + 1; ++i )); do
    my_port=$(($scheduler_port + $i))
    if (( $i <= $num_servers )); then
        my_role="SERVER"
        my_id="S$i"
    else
        my_role="WORKER"
        my_id="W$i"
    fi
    env="\
        -e cloud_provider=$cloud_provider \
        -e scheduler_host=$scheduler_host \
        -e scheduler_port=$scheduler_port \
        -e my_role=$my_role \
        -e my_port=$my_port \
        -e my_id=$my_id \
        -e num_servers=$num_servers \
        -e num_workers=$num_workers \
        -e app_file=$app_file \
        "
    docker rm n$i > /dev/null 2>&1
    docker run -d  -p $my_port:$my_port $env -e "args=$args" $mount --name n$i $image &
done

wait

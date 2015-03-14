#!/bin/bash

# Part 1 : command parsing functions
show_help() {
cat << EOF
Usage: ps.sh [start|add|kill|stop] [args]
start 		start an application
    -hostfile 	a list of ip address of machines. parameter server nodes are
evenly assigned to these machines. #nodes could > #machines. if it is empty, generate a file with one line "127.0.0.1"
    -worker     number of worker nodes
    -server     number of server nodes
    -bin     	binary to launch, eg:../build/linear
    -arg 	other arguments, eg:"-app_file l1lr.conf"

ls 		print a list of running nodes with format "node_id server/worker/scheduler machine:port bin arg"

add 		add nodes
    -worker     number of worker nodes to add
    -server     number of server nodes to add

kill 		kill a node
    [nodeid]     id of scheduler/worker/server node to kill

stop 		stop app
EOF
}

show_empty() {
	echo "ERROR: Must specify a non-empty $1 argument." >&2
	exit 1
}

show_file_not_exist() {
	echo "ERROR: $1 not exist." >&2
	exit 1
}

check_start() {
	if [[ ! $worker ]]; then
	    show_empty '-worker'
	fi
	if [[ ! $server ]]; then
	    show_empty '-server'
	fi
	if [[ ! $bin ]]; then
	    show_empty '-bin'
	fi
	# if [[ ! $arg ]]; then
	#     show_empty '-arg'
	# fi
	if [[ ! $hostfile ]];
	then
	    echo '127.0.0.1' > ps_default_hostfile
	    hostfile='ps_default_hostfile'
	elif [[ ! -f $hostfile ]]; then
		show_file_not_exist $hostfile
	fi

	# save hosts
	if [[ ! -d /tmp/ps-state ]]; then
		mkdir -p /tmp/ps-state
	fi
	cp $hostfile /tmp/ps-state/hosts
}

check_add() {
	if [[ ! $new_worker ]] && [[ ! $new_server ]]; then
	    show_empty '-worker or -server'
	fi
}

check_kill() {
	if [[ ! $node ]]; then
	    show_empty 'node'
	fi
}
# Part 2 : command excecution functions

init_state() {
	# $1 host
	ssh $1 "if [[ ! -d /tmp/ps-state ]];then mkdir -p /tmp/ps-state;fi;
	if [[ -f /tmp/ps-state/nodes ]];then rm -f /tmp/ps-state/nodes;fi;" < /dev/null
}

list_state() {
	# $1 host
	ssh $1 "if [[ -f /tmp/ps-state/nodes ]];then cat /tmp/ps-state/nodes;fi;" &
}

clear_state() {
	# $1 host
	ssh $1 "if [[ -f /tmp/ps-state/nodes ]];then rm -f /tmp/ps-state/nodes;fi;" &
}


get_port() {
	# $1 host
	port=$(($RANDOM%(32767-1023)+1024))
	ssh $1 "/usr/bin/env lsof -i tcp:$port" >> /dev/null
	while [[ $? -lt 1 ]]; do
		port=$(($RANDOM%(32767-1023)+1024))
		ssh $1 "/usr/bin/env lsof -i tcp:$port" >> /dev/null
	done
}

start_scheduler() {
	# $1 host
	get_port $1
	# start scheduler
	Sch="\"role:SCHEDULER,hostname:'$1',port:$port,id:'H'\""
	ssh $1 "cd $(pwd) && $bin -my_node $Sch -scheduler $Sch -num_workers $worker -num_servers $server $arg &
	echo -e \"H\tscheduler\t$1:$port\t"'$!'"\" >> /tmp/ps-state/nodes" &
	echo "$bin -my_node $Sch -scheduler $Sch -num_workers $worker -num_servers $server $arg"
	# save parameter to facilitate add operation
	echo -e "$bin\t$Sch\t$server\t$worker\t$arg" > /tmp/ps-state/args
}

start_worker() {
	# $1 host, $2 id
	get_port $1
	# start scheduler
	N="\"role:WORKER,hostname:'$1',port:$port,id:'W$2'\""
	ssh $1 "cd $(pwd) && $bin -my_node $N -scheduler $Sch -num_workers $worker -num_servers $server $arg &
	echo -e \"W$2\tworker\t$1:$port\t"'$!'"\" >> /tmp/ps-state/nodes" &
	echo "$bin -my_node $N -scheduler $Sch -num_workers $worker -num_servers $server $arg"
}

start_server() {
	# $1 host, $2 id
	get_port $1
	# start scheduler
	N="\"role:SERVER,hostname:'$1',port:$port,id:'S$2'\""
	ssh $1 "cd $(pwd) && $bin -my_node $N -scheduler $Sch -num_workers $worker -num_servers $server $arg &
	echo -e \"S$2\tserver\t$1:$port\t"'$!'"\" >> /tmp/ps-state/nodes" &
	echo "$bin -my_node $N -scheduler $Sch -num_workers $worker -num_servers $server $arg"
}

reset_counter() {
	# reset circle counter
	counter=0
}

inc_counter() {
	# $1 circle size
	counter=$((counter+1))
	if [[ counter -eq $1 ]];
	then
		counter=0
	fi
}

read_hosts() {
	# read hosts in array
	# $1 first time
	num_hosts=0
	while read line
	do
	    hosts[num_hosts]=$line
	    num_hosts=$((num_hosts+1))
	    # init state file on this machine
	    if [[ $1 -eq 1 ]]; then
	    	init_state $line
	    fi
	done < /tmp/ps-state/hosts
}

read_args() {
	bin=$(cat /tmp/ps-state/args | awk '{print $1}')
	Sch=$(cat /tmp/ps-state/args | awk '{print $2}')
	server=$(cat /tmp/ps-state/args | awk '{print $3}')
	worker=$(cat /tmp/ps-state/args | awk '{print $4}')
	arg=$(cat /tmp/ps-state/args | awk '{print $5}')
}

start() {
	read_hosts 1
	reset_counter
	# launch scheduler
	start_scheduler ${hosts[counter]}
	inc_counter ${num_hosts}
	# launch worker nodes one by one
	for (( i = 0; i < $worker; i++ )); do
	   	start_worker ${hosts[counter]} $i
		inc_counter ${num_hosts}
	done
	# launch server nodes one by one
	for (( i = 0; i < $server; i++ )); do
	   	start_server ${hosts[counter]} $i
		inc_counter ${num_hosts}
	done

}

list() {
	read_hosts 0
	for (( i = 0; i < $num_hosts; i++ )); do
	   	list_state ${hosts[$i]}
	done
}

kill_node() {
	# $1 worker id
	list > /tmp/ps-state/nodes
	wait
	pid=$(grep $1 /tmp/ps-state/nodes | awk '{print $4}')
	host=$(grep $1  /tmp/ps-state/nodes | awk '{print $3}' | awk -F: '{print $1}')
	ssh $host "kill -9 $pid && sed -i '/$1/d' /tmp/ps-state/nodes"
}

get_next_node_id_and_machine() {
	# $1 role
	list > /tmp/ps-state/nodes
	wait
	if [[ $1 = 'worker' ]];
	then
		next_node_id=$(grep W /tmp/ps-state/nodes | awk '{print $1}' | grep -o [0-9].* | sort -gr | head -n 1)
		next_node_id=$((next_node_id+1))
	elif [[ $1 = 'server' ]];
	then
		next_node_id=$(grep S /tmp/ps-state/nodes | awk '{print $1}' | grep -o [0-9].* | sort -gr | head -n 1)
		next_node_id=$((next_node_id+1))
	fi

	# get machine with least load
	least_load=10000
	for (( i = 0; i < $num_hosts; i++ )); do
		tmp=$(grep ${hosts[i]} /tmp/ps-state/nodes | wc -l)
		if [[ $tmp -lt $least_load ]]; then
			least_load=$tmp
			next_node_machine=${hosts[i]}
		fi
	done
}

add_nodes() {
	read_args
	read_hosts 0
	if [[ $new_worker ]];
	then
		for (( j = 0; j < $new_worker; j++ )); do
			get_next_node_id_and_machine 'worker'
			start_worker $next_node_machine $next_node_id
		done
	fi
	if [[ $new_server ]];
	then
		for (( j = 0; j < $new_server; j++ )); do
			get_next_node_id_and_machine 'server'
			start_server $next_node_machine $next_node_id
		done
	fi
}

clear() {
	for (( i = 0; i < $num_hosts; i++ )); do
	   	clear_state ${hosts[$i]}
	done
}

stop() {
	list > /tmp/ps-state/nodes
	wait
	while read line;
	do
		pid=$(echo $line | awk '{print $4}')
		host=$(echo $line | awk '{print $3}' | awk -F: '{print $1}')
		ssh $host "kill -9 $pid" < /dev/null
	done < /tmp/ps-state/nodes

	#read hosts for clear
	read_hosts 0
	clear
	wait
}


# Part 3 : main

if [[ $# -lt 1 ]] ; then
	show_help
	exit -1
fi

cmd=$1


if [[ $1 = 'start' ]]; then
	shift
	while [[ $# -gt 0 ]]; do
	    case $1 in
	        -h|-\?|--help)   # Call a "show_help" function to display a synopsis, then exit.
	            show_help
	            exit
	            ;;
	        -hostfile)       # Takes an option argument, ensuring it has been specified.
	            if [[ $# -gt 1 ]] && ! [[ $2 = -?* ]]; then
	                hostfile=$2
	                shift 2
	                continue
	            else
	                show_empty '-hostfile'
	            fi
	            ;;
	        -worker)
	            if [[ $# -gt 1 ]] && ! [[ $2 = -?* ]]; then
	                worker=$2
	                shift 2
	                continue
	            else
	                show_empty '-worker'
	            fi
	            ;;
	        -server)
	            if [[ $# -gt 1 ]] && ! [[ $2 = -?* ]]; then
	                server=$2
	                shift 2
	                continue
	            else
	                show_empty '-server'
	            fi
	            ;;
	        -bin)
	            if [[ $# -gt 1 ]] && ! [[ $2 = -?* ]]; then
	                bin=$2
	                shift 2
	                continue
	            else
	                show_empty '-bin'
	            fi
	            ;;
	        -arg)
	            if [[ $# -gt 1 ]] ; then
	                arg=$2
	                shift 2
	                continue
	            else
	                show_empty '-arg'
	            fi
	            ;;
	        -?*)
	            printf 'WARN: Unknown option (ignored): %s\n' $1 >&2
	            ;;
	        *)               # Default case: If no more options then break out of the loop.
	            break
	    esac

	    shift
	done
	check_start
	start
	wait
	clear
	wait
elif [[ $1 = 'stop' ]]; then
	shift
	while [[ $# -gt 0 ]]; do
	    case $1 in
	        -h|-\?|--help)
	            show_help
	            exit
	            ;;
	        -?*)
	            printf 'WARN: Unknown option (ignored): %s\n' $1 >&2
	            ;;
	        *)
	            break
	    esac

	    shift
	done
	stop
elif [[ $1 = 'ls' ]]; then
	shift
	while [[ $# -gt 0 ]]; do
	    case $1 in
	        -h|-\?|--help)
	            show_help
	            exit
	            ;;
	        -?*)
	            printf 'WARN: Unknown option (ignored): %s\n' $1 >&2
	            ;;
	        *)
	            break
	    esac

	    shift
	done
	list
	wait
elif [[ $1 = 'add' ]]; then
	shift
	while [[ $# -gt 0 ]]; do
	    case $1 in
	        -h|-\?|--help)
	            show_help
	            exit
	            ;;
	         -worker)
	            if [[ $# -gt 1 ]] && ! [[ $2 = -?* ]]; then
	                new_worker=$2
	                shift 2
	                continue
	            else
	                show_empty '-worker'
	            fi
	            ;;
	         -server)
	            if [[ $# -gt 1 ]] && ! [[ $2 = -?* ]]; then
	                new_server=$2
	                shift 2
	                continue
	            else
	                show_empty '-server'
	            fi
	            ;;
	        -?*)
	            printf 'WARN: Unknown option (ignored): %s\n' $1 >&2
	            ;;
	        *)
	            break
	    esac

	    shift
	done
	check_add
	add_nodes

elif [[ $1 = 'kill' ]]; then
	shift
	while [[ $# -gt 0 ]]; do
	    case $1 in
	        -h|-\?|--help)
	            show_help
	            exit
	            ;;
	         H|W*|S*)
	            node=$1
	            shift 1
	            ;;
	        -?*)
	            printf 'WARN: Unknown option (ignored): %s\n' $1 >&2
	            ;;
	        *)
	            break
	    esac

	    shift
	done
	check_kill
	kill_node $node
else
	show_help
fi

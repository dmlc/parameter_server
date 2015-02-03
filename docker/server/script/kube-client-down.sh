#!/bin/bash
while read line
do
	stop_all="docker stop \$(docker ps -a -q)"
	rm_all="docker rm \$(docker ps -a -q)"
	if [ $# -gt 0 ]; then
		ssh -i $1 ${line} ${stop_all} < /dev/null
		ssh -i $1 ${line} ${rm_all} < /dev/null
	else
		ssh ${line} ${stop_all} < /dev/null
                ssh ${line} ${rm_all} < /dev/null
	fi
done

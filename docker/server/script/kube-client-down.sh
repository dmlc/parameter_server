#!/bin/bash
while read line
do
	stop_all="docker stop \$(docker ps -a -q)"
	rm_all="docker rm \$(docker ps -a -q)"
	ssh ${line} ${stop_all} < /dev/null
	ssh ${line} ${rm_all} < /dev/null
done

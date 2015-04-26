#!/bin/bash
if [[ $my_host ]]; then
	echo "$my_host"
else
	case $cloud_provider in
	    amazonec2) 
	        cat /var/log/cloud-init.log | awk '/update hostname/ {print $10}'
	    ;;
	    *) 
	        echo "Currently only support amazonec2!"
	        exit -1 
	    ;;
	esac
fi



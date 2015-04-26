#!/bin/bash
if [[ $# -lt 2 ]]; then
    echo "usage: $0 local_file s3_path"
    exit -1
fi

local_file=$1
shift

if [[ ! -e $local_file ]]; then
	echo "$local_file does not exist!"
	exit -1
fi

s3_path=$1
shift

bucket=`echo $s3_path | cut -d'/' -f3`
path=${s3_path#s3://$bucket}

length=`stat $local_file | awk '{print $8}'`
curl "http://$bucket.s3.amazonaws.com$path?Content-Length=$length&x-amz-acl=public-read" --upload-file $local_file



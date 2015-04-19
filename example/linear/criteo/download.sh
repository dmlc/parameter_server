#!/bin/bash

dir=`dirname "$0"`
cd $dir/../data


if [ ! -f train.txt ]; then
    if [ ! -f dac.tar.gz ]; then
        wget https://s3-eu-west-1.amazonaws.com/criteo-labs/dac.tar.gz
    fi
    tar -zxvf dac.tar.gz
fi


name=(train test)
for t in "${name[@]}"
do
    echo $t
    mkdir -p criteo/${t}

    # shuffle
    # shuf $t.txt  >rnd

    # split
    split -n l/16 --numeric-suffixes=1 --suffix-length=3 $t.txt criteo/${t}/part-
done

# rm rnd

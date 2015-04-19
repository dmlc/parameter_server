#!/bin/bash

dir=`dirname "$0"`
cd $dir/../data


if [ ! -f train.txt ]; then
    if [ ! -f dac.tar.gz ]; then
        wget https://s3-eu-west-1.amazonaws.com/criteo-labs/dac.tar.gz
    fi
    tar -zxvf dac.tar.gz
fi

echo "split train.txt..."
mkdir -p criteo/train
split -n l/18 --numeric-suffixes=1 --suffix-length=3 train.txt criteo/train/part-

echo "make a test set"
mkdir -p criteo/test
mv criteo/train/part-01[7-8] criteo/test

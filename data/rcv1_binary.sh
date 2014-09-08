#!/bin/bash

if ! [ -e rcv1_train.binary ]; then
    wget http://www.csie.ntu.edu.tw/~cjlin/libsvmtools/datasets/binary/rcv1_train.binary.bz2
    bunzip2 rcv1_train.binary.bz2
    # downloading the test set costs >20min
# wget http://www.csie.ntu.edu.tw/~cjlin/libsvmtools/datasets/binary/rcv1_test.binary.bz2
fi


split -l 1000 rcv1_train.binary rcv1_train_

train=rcv1/train
test=rcv1/test
mkdir -p $train
mkdir -p $test

rm -f $train/* $test/*

i=0;
for f in rcv1_train_*
do
    dir=$train
    if (($i > 12))
    then
        dir=$test
    fi
    # text format
    mv ${f} $dir/part-`printf %03d $i`

    # protobuf format
    # ../bin/text2proto -format libsvm -input ${f} -output $dir/part-`printf %03d $i`
    ((i++))
done

rm -f rcv1_train_*

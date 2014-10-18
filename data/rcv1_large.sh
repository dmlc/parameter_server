#!/bin/bash
# split rcv1_train.binary into a test set while split rcv1_test.bianry,
# which is much bigger, into a training set

if ! [ -e rcv1_train.binary ]; then
    wget http://www.csie.ntu.edu.tw/~cjlin/libsvmtools/datasets/binary/rcv1_train.binary.bz2
    bunzip2 rcv1_train.binary.bz2
fi

if ! [ -e rcv1_test.binary ]; then
    wget http://www.csie.ntu.edu.tw/~cjlin/libsvmtools/datasets/binary/rcv1_test.binary.bz2
    bunzip2 rcv1_test.binary.bz2
fi

train=rcv1/train
test=rcv1/test
mkdir -p $train
mkdir -p $test

split -l 42338 rcv1_test.binary test_
i=0;
for f in test_*
do
    d=${train}/part-`printf %03d $i`
    mv ${f} ${d}
    gzip ${d}
    ((i++))
done


split -l 5100 rcv1_train.binary train_
j=0;
for f in train_*
do
    d=${test}/part-`printf %03d $j`
    mv ${f} ${d}
    gzip ${d}
    ((i++))
done

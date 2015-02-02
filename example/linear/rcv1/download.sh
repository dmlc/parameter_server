#!/bin/bash

dir=`dirname "$0"`
cd $dir/../data

# download
if ! [ -e rcv1_train.binary ]; then
    wget http://www.csie.ntu.edu.tw/~cjlin/libsvmtools/datasets/binary/rcv1_train.binary.bz2
    bunzip2 rcv1_train.binary.bz2
fi

if ! [ -e rcv1_test.binary ]; then
    wget http://www.csie.ntu.edu.tw/~cjlin/libsvmtools/datasets/binary/rcv1_test.binary.bz2
    bunzip2 rcv1_test.binary.bz2
fi


name=(train test)
for t in "${name[@]}"
do
    echo $t
    # shuffle
    rnd=rcv1_${t}_rand
    shuf rcv1_${t}.binary >$rnd

    # split
    mkdir -p rcv1/${t}
    rm -f rcv1/${t}/*
    split -n l/8 --numeric-suffixes=1 --suffix-length=3 $rnd rcv1/${t}/part-
    rm $rnd
done

# swap train and test
mv rcv1/train tmp
mv rcv1/test rcv1/train
mv tmp rcv1/test

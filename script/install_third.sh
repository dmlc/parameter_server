#!/bin/bash
dir=`dirname "$0"`
cd $dir/..
git clone https://github.com/hjk41/third_party
cd third_party
./install.sh

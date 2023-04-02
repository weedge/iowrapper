#!/usr/bin/env bash

set -e

nproc=${1:-"1"}

apt-get update

apt-get install -y make
apt-get install -y git 
apt-get install -y gcc g++
apt-get install -y fio
cd /tmp

# for man liburing  help doc
git clone https://github.com/axboe/liburing.git
cd liburing
./configure --cc=gcc --cxx=g++
make -j$(nproc)
make install
cd -


#!/bin/bash
set -e

CURRENT_DIR=$(cd `dirname $0`; pwd)
cd $CURRENT_DIR

cc=gcc
build_dir=../build

for i in `ls *.c | sed -r "s/(.*).c/\1/g"`;do
    if [[ "$i" =~ iouring ]]; then
        $cc $i.c -g -Wall -O0 -D_GNU_SOURCE -luring -o $build_dir/$i 
    else 
        $cc $i.c -g -Wall -O0 -D_GNU_SOURCE -o $build_dir/$i
    fi
done


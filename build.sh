#!/bin/bash

set -e
set -u

if [ ! -d `pwd`/build ];then
    mkdir `pwd`/build
fi

cd `pwd`/build && cmake .. && make
cd ..
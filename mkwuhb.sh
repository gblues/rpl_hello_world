#!/usr/bin/env bash
script_dir=$(cd $(dirname $0) && pwd)

mkdir -p $script_dir/lib/build
cd $script_dir/lib/build
if [ ! -e Makefile ]; then
  wiiu-cmake ../libhello
fi

make clean && make
cd $script_dir/
make clean
mkdir -p content
mv $script_dir/lib/build/*.rpl content

make V=1

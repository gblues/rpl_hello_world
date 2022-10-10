#!/usr/bin/env bash
script_dir=$(cd $(dirname $0) && pwd)

mkdir -p $script_dir/lib/build
cd $script_dir/lib/build
if [ ! -e Makefile ]; then
  cmake ../libhello
fi

make clean && make
cd $script_dir/
make clean && make

mkdir -p $script_dir/build/content
cp $script_dir/lib/build/libhello.rpl $script_dir/build/content

wuhbtool $script_dir/rpl_hello_world.rpx $script_dir/wut_dlink_demo.wuhb --content=$script_dir/build/content --name="WUT Dynamic Link Demo" --short-name="WUT-dlopen" --author="gblues"

cd $script_dir

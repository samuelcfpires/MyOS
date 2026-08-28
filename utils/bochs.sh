#!/bin/sh
set -e

./iso.sh
./disk.sh
cd ..


debug_arg="-debugger"

if [ "$1" == "-g" ]; then
	debug_arg="-dbg_gui"
elif [ -n "$1" ]; then
	echo "Options:"
	echo "  -g          Use GUI debugger"
fi


bochs -f utils/bochs/bochsrc.txt -q $debug_arg

#!/bin/sh

export | grep CROSS_COMPILE > /dev/null
if [ $? -ne 0 ]; then
   export CROSS_COMPILE=aarch64-none-elf-
fi

cmake -DCMAKE_BUILD_TYPE=Release -DRMM_CONFIG=fvp_defcfg -DLOG_LEVEL=40 -B build
cmake --build build



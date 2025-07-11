#!/bin/sh

if [ $# -ne 1 ]; then
   echo "./build fvp|qemu"
   exit 1
fi

# Environment variable for cross compiler
export | grep CROSS_COMPILE > /dev/null
if [ $? -ne 0 ]; then
   export CROSS_COMPILE=aarch64-none-elf-
fi

# Environment variable for parallel build
export | grep CMAKE_BUILD_PARALLEL_LEVEL > /dev/null
if [ $? -ne 0 ]; then
   export CMAKE_BUILD_PARALLEL_LEVEL=8
fi

if [ "$1" == "qemu" ]; then
   cmake -DCMAKE_BUILD_TYPE=Debug       \
         -DRMM_CONFIG=qemu_virt_defcfg  \
         -DRMM_MAX_GRANULES=0x400000    \
         -DLOG_LEVEL=40                 \
         -DAPP_FW_LOGGING=0             \
         -B build-qemu
   cmake --build build-qemu
elif [ "$1" == "fvp" ]; then
   cmake -DCMAKE_BUILD_TYPE=Debug       \
         -DRMM_CONFIG=fvp_defcfg        \
         -DLOG_LEVEL=40                 \
         -DAPP_FW_LOGGING=0             \
         -B build
   cmake --build build
else
   echo "./build.sh fvp|qemu"
   exit 1
fi

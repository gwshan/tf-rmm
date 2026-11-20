#!/bin/sh

if [ $# -ne 1 ]; then
   echo "./build fvp|qemu"
   exit 1
fi

export | grep CROSS_COMPILE > /dev/null
if [ $? -ne 0 ]; then
   export CROSS_COMPILE=aarch64-none-elf-
fi

#
# [1] -DRMM_EL3_COMPAT_RESERVE_MEM=OFF
#     All memory requests from RMM will be routed to TF-A running at EL3 where
#     the memory pool should have been properly built.
#
# [2] -DRMM_MAX_SIZE=0x600000
#     Maximum size of RMM image is limited to the size specified by TF-A
#     (PLAT_ARM_RMM_PAYLOAD_SIZE).
#
#
if [ "$1" == "qemu" ]; then
   cmake -DCMAKE_BUILD_TYPE=Debug           \
         -DRMM_CONFIG=qemu_virt_defcfg      \
         -DRMM_MAX_GRANULES=0x400000        \
         -DRMM_EL3_COMPAT_RESERVE_MEM=OFF   \
         -DRMM_MAX_SIZE=0x600000            \
         -DLOG_LEVEL=40                     \
         -B build-qemu
   cmake --build build-qemu
elif [ "$1" == "fvp" ]; then
   cmake -DCMAKE_BUILD_TYPE=Debug       \
         -DRMM_CONFIG=fvp_defcfg        \
         -DLOG_LEVEL=40                 \
         -B build
   cmake --build build
else
   echo "./build.sh fvp|qemu"
   exit 1
fi

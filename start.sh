#!/bin/bash
PATH=${PATH}:~/toolchains/UBERTC/aarch64-linux-android-5.3/bin
export ARCH=arm64
make aio_otfp_m_defconfig ARCH=arm64 CROSS_COMPILE=aarch64-linux-android-
make -j5 ARCH=arm64 CROSS_COMPILE=aarch64-linux-android-
./complete.sh

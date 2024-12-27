#!/bin/bash

export ARCH=arm64
export CROSS_COMPILE=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-
mkdir output

make -C $(pwd) O=output VARIANT_DEFCONFIG=msm8916_sec_j5xlte_eur_defconfig msm8916_sec_defconfig SELINUX_DEFCONFIG=selinux_defconfig
make -C $(pwd) O=output

cp output/arch/arm64/boot/Image $(pwd)/arch/arm64/boot/Image


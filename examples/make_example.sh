#!/bin/sh

BASEDIR=$(cd $(dirname $0) ; pwd -P)
export HOST=$BASEDIR/../toolchain/bin/arm-linux-androideabi

# To solve problems with std::future
# https://lists.debian.org/debian-arm/2013/12/msg00007.html
# http://stackoverflow.com/questions/22036396/stdpromise-error-on-cross-compiling
export EXTRA_CXXFLAGS+=" -march=armv7-a -mtune=cortex-a9 -mfpu=neon"
export EXTRA_CXXFLAGS+=" --sysroot=${BASEDIR}/../toolchain/sysroot" 
export EXTRA_LDFLAGS+=" -march=armv7-a -Wl,--fix-cortex-a8"
export EXTRA_LDFLAGS+=" -lstdc++"

# Some STL functions are not availiable with Android
# To bypass, define CYGWIN
export EXTRA_CXXFLAGS+=" -DCYGWIN"

# Set TARGET_OS manually
export TARGET_OS="OS_ANDROID_CROSSCOMPILE"

export CC="${HOST}-gcc"
export CXX="${HOST}-g++"
export LD="${HOST}-ld"
export AR="${HOST}-ar"
export NM="${HOST}-nm"
export AS="${HOST}-as"
export RANLIB="${HOST}-ranlib"
export STRIP="${HOST}-strip"
export OBJCOPY="${HOST}-objcopy"
export OBJDUMP="${HOST}-objdump"

echo "CXX: ${CXX}"

PORTABLE=1 make all

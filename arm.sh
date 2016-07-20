#!/bin/bash

if [ -z "$NDK" ]; then
	echo "set up \$NDK PATH" >&2
	exit 1
fi

BASEDIR=$(cd $(dirname $0) ; pwd -P)
TOOLCHAINDIR=$BASEDIR/../toolchain-zynq
GFLAGSDIR=$BASEDIR/../gflags-zynq

# toolchain/gflags: outside rocksdb (due to "make clean" issue)

if [ ! -d $TOOLCHAINDIR ]; then
	$NDK/build/tools/make-standalone-toolchain.sh --arch=arm --platform=android-16 --system=linux-x86_64 --install-dir=$TOOLCHAINDIR --stl=gnustl
	cat toolchain.patch | (cd ..; patch -p1)
fi

if [ ! -d $GFLAGSDIR ]; then
	(cd ..; git clone https://github.com/cwchung90/gflags gflags-zynq; \
	cd gflags-zynq; sh AndroidSetup.sh)
fi

# to support some STL functions which were not implemented in android-18 library
export CPATH=$CPATH:$BASEDIR/include
export EXTRA_CXXFLAGS="$EXTRA_CXXFLAGS -DROCKSDB_LITE"

# to support gflags
export EXTRA_CXXFLAGS="$EXTRA_CXXFLAGS -DGFLAGS=google"
export EXTRA_LDFLAGS="$EXTRA_LDFLAGS -L$GFLAGSDIR/build/lib -lgflags"

export HOST=$TOOLCHAINDIR/bin/arm-linux-androideabi


# To solve problems with std::future
# https://lists.debian.org/debian-arm/2013/12/msg00007.html
# http://stackoverflow.com/questions/22036396/stdpromise-error-on-cross-compiling
export EXTRA_CXXFLAGS+=" -Wno-format -Wno-maybe-uninitialized"
export EXTRA_CXXFLAGS+=" -march=armv7-a -mtune=cortex-a9 -mfpu=neon"
export EXTRA_CXXFLAGS+=" --sysroot=$TOOLCHAINDIR/sysroot"

export EXTRA_LDFLAGS+=" -march=armv7-a -Wl,--fix-cortex-a8"
export EXTRA_LDFLAGS+=" -lstdc++ -lsupc++"


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

#PORTABLE=1 make shared_lib
#PORTABLE=1 make release -j8
#PORTABLE=1 DEBUG_LEVEL=0 make static_lib -j8
PORTABLE=1 make db_test -j8
#PORTABLE=1 DEBUG_LEVEL=0 make all -j8

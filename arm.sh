#!/bin/sh


if [ -z "$NDK" ]; then
	echo "set up \$NDK PATH" >&2
	exit 1
fi

BASEDIR=$(dirname $0)
if [ ! -d $BASEDIR/toolchain ]; then
	$NDK/build/tools/make-standalone-toolchain.sh --arch=arm --platform=android-21 --install-dir=$BASEDIR/toolchain --toolchain=arm-linux-androideabi-4.8 --stl=gnustl

	# To solve timed_mutex
	# _GTHREAD_USE_MUTEX_TIMEDLOCK=0 -> 1
	# $TOOLCHAIN/include/c++/4.8/arm-linux-androideabi/ ... / c++config.h
	patch -p0 -i toolchain.patch
fi


export HOST=$BASEDIR/toolchain/bin/arm-linux-androideabi

# To solve problems with std::future
# https://lists.debian.org/debian-arm/2013/12/msg00007.html
# http://stackoverflow.com/questions/22036396/stdpromise-error-on-cross-compiling
export EXTRA_CXXFLAGS+=" -march=armv7-a -mtune=cortex-a9 -mfpu=neon"
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

#PORTABLE=1 make release
PORTABLE=1 make shared_lib

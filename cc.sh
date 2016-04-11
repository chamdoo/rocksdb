BASEDIR=$(pwd -P)
TOOLCHAINDIR=$BASEDIR/../toolchain

# to support some STL functions which were not implemented in android-18 library
export CPATH=$CPATH:$BASEDIR/include
export EXTRA_CXXFLAGS="$EXTRA_CXXFLAGS -DROCKSDB_LITE"

# to support gflags: should be installed @ ../gflags (compiled with namespace google)
export EXTRA_CXXFLAGS="$EXTRA_CXXFLAGS -DGFLAGS=google"
export EXTRA_LDFLAGS="$EXTRA_LDFLAGS -lgflags -L../gflags/build/lib"

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

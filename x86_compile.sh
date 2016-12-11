#!/bin/bash

# RocksDB for x86-Ubuntu depends on gflags, snappy, zlib, bzip2. 
# gflags: sudo apt-get install libgflags-dev

export OPT+=" -DROCKSDB_LITE"

export EXTRA_CXXFLAGS+=" -DNOHOST" # TO TURN ON NOHOST. Below (ENABLE_FLASH_DB) works only if NOHOST is on

export EXTRA_CXXFLAGS+=" -DENABLE_FLASH_DB"
#export EXTRA_CXXFLAGS+=" -DENABLE_FLASH_DB"
#export EXTRA_CXXFLAGS+=" -DENABLE_READ_DEBUG"

#PORTABLE=1 make db_test -j8 V=1
PORTABLE=1 make db_bench -j8 V=1

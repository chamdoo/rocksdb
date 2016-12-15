#! /bin/bash

#merge_operator = uint64add, stringappend
#compression_type = none, snappy, zlib, bzip2, lz4, lz4hc, zstd
#see details rocksdb/tools/db_bench_tool.cc
./db_bench --benchmarks=fillseq,fillsync,fillrandom,overwrite,readrandom,newiterator,newiteratorwhilewriting,seekrandom,seekrandomwhilewriting,seekrandomwhilemerging,readseq,readreverse,readrandom,multireadrandom,readseq,readtocache,readreverse,readwhilewriting,readwhilemerging,readrandomwriterandom,updaterandom,randomwithverify,fill100K,crc32c,xxhash,compress,uncompress,acquireload,fillseekseq,randomreplacekeys --merge_operator=uint64add --compression_type=none


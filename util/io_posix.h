//  Copyright (c) 2013, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
#pragma once
#include <unistd.h>
#include "rocksdb/env.h"

// For non linux platform, the following macros are used only as place
// holder.
#if !(defined OS_LINUX) && !(defined CYGWIN)
#define POSIX_FADV_NORMAL 0     /* [MC1] no further special treatment */
#define POSIX_FADV_RANDOM 1     /* [MC1] expect random page refs */
#define POSIX_FADV_SEQUENTIAL 2 /* [MC1] expect sequential page refs */
#define POSIX_FADV_WILLNEED 3   /* [MC1] will need these pages */
#define POSIX_FADV_DONTNEED 4   /* [MC1] dont need these pages */
#endif

namespace rocksdb {

static Status IOError(const std::string& context, int err_number) {
  return Status::IOError(context, strerror(err_number));
}

class PosixSequentialFile : public SequentialFile {
 private:
  std::string filename_;
  int fd_;
  bool use_os_buffer_;
  NoHostFs* nohost_;

 public:
  PosixSequentialFile(const std::string& fname, int fd,
                      const EnvOptions& options, NoHostFs* nohost);
  virtual ~PosixSequentialFile();

  virtual Status Read(size_t n, Slice* result, char* scratch) override;
  virtual Status Skip(uint64_t n) override;
  virtual Status InvalidateCache(size_t offset, size_t length) override;
};

class PosixRandomAccessFile : public RandomAccessFile {
 private:
  std::string filename_;
  int fd_;
  bool use_os_buffer_;
  NoHostFs* nohost_;

 public:
  PosixRandomAccessFile(const std::string& fname, int fd,
                        const EnvOptions& options, NoHostFs* nohost);
  virtual ~PosixRandomAccessFile();

  virtual Status Read(uint64_t offset, size_t n, Slice* result,
                      char* scratch) const override;
#ifdef OS_LINUX
  virtual size_t GetUniqueId(char* id, size_t max_size) const override;
#endif
  virtual void Hint(AccessPattern pattern) override;
  virtual Status InvalidateCache(size_t offset, size_t length) override;
};

class PosixWritableFile : public WritableFile {
 private:
  const std::string filename_;
  int fd_;
  uint64_t filesize_;
  NoHostFs* nohost_;
#ifdef ROCKSDB_FALLOCATE_PRESENT
  bool allow_fallocate_;
  bool fallocate_with_keep_size_;
#endif

 public:
  PosixWritableFile(const std::string& fname, int fd,
                    const EnvOptions& options, NoHostFs* nohost);
  ~PosixWritableFile();

  // Means Close() will properly take care of truncate
  // and it does not need any additional information
  virtual Status Truncate(uint64_t size) override { return Status::OK(); }
  virtual Status Close() override;
  virtual Status Append(const Slice& data) override;
  virtual Status Flush() override;
  virtual Status Sync() override;
  virtual Status Fsync() override;
  virtual bool IsSyncThreadSafe() const override;
  virtual uint64_t GetFileSize() override;
  virtual Status InvalidateCache(size_t offset, size_t length) override;
#ifdef ROCKSDB_FALLOCATE_PRESENT
  virtual Status Allocate(off_t offset, off_t len) override;
  virtual Status RangeSync(off_t offset, off_t nbytes) override;
  virtual size_t GetUniqueId(char* id, size_t max_size) const override;
#endif
};


class PosixDirectory : public Directory {
 public:
  explicit PosixDirectory(int fd, NoHostFs* nohost) : fd_(fd), nohost_(nohost) {}
  ~PosixDirectory();
  virtual Status Fsync() override;

 private:
  int fd_;
  NoHostFs* nohost_;
};

}  // namespace rocksdb

//  Copyright (c) 2013, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

//#ifdef ROCKSDB_LIB_IO_POSIX

#include <iostream>
using namespace std;
#include "util/io_posix.h"
#include <errno.h>
#include <fcntl.h>
#if defined(OS_LINUX)
#include <linux/fs.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef OS_LINUX
#include <sys/statfs.h>
#include <sys/syscall.h>
#endif
#include "port/port.h"
#include "rocksdb/slice.h"
#include "util/coding.h"
#include "util/iostats_context_imp.h"
#include "util/posix_logger.h"
#include "util/string_util.h"
#include "util/sync_point.h"

namespace rocksdb {

// A wrapper for fadvise, if the platform doesn't support fadvise,
// it will simply return Status::NotSupport.
int Fadvise(int fd, off_t offset, size_t len, int advice) {
#ifdef OS_LINUX
  return posix_fadvise(fd, offset, len, advice);
#else
  return 0;  // simply do nothing.
#endif
}

/*
 * PosixSequentialFile
 */
PosixSequentialFile::PosixSequentialFile(const std::string& fname, int fd,
                                         const EnvOptions& options, NoHostFs* nohost)
    : filename_(fname),
      fd_(fd),
      use_os_buffer_(options.use_os_buffer),
	  nohost_(nohost){}

PosixSequentialFile::~PosixSequentialFile() { nohost_->Close(fd_);}

Status PosixSequentialFile::Read(size_t n, Slice* result, char* scratch) {
  Status s;
  size_t r = 0;
  r = nohost_->SequentialRead(fd_, scratch, n);

  *result = Slice(scratch, r);
  if (r < n) {
    if (nohost_->IsEof(fd_)) {
      // We leave status as ok if we hit the end of the file
      // We also clear the error so that the reads can continue
      // if a new data is written to the file
    	s = Status::OK();
    } else {
      // A partial read with an error: return a non-ok status
      s = IOError(filename_, errno);
    }
  }/*
  if (!use_os_buffer_) {
    // we need to fadvise away the entire range of pages because
    // we do not want readahead pages to be cached.
    Fadvise(fd_, 0, 0, POSIX_FADV_DONTNEED);  // free OS pages
  }*/
  return s;

}

Status PosixSequentialFile::Skip(uint64_t n) {
  if (nohost_->Lseek(fd_, n) < 0) {
    return IOError(filename_, errno);
  }
  return Status::OK();

}

Status PosixSequentialFile::InvalidateCache(size_t offset, size_t length) {
#ifndef OS_LINUX
  return Status::OK();
#else
  // free OS pages
  int ret = Fadvise(fd_, offset, length, POSIX_FADV_DONTNEED);
  if (ret == 0) {
    return Status::OK();
  }
  return IOError(filename_, errno);
#endif
#ifdef NOHOST
	/* Do Nothing */
#endif
}

#if defined(OS_LINUX)
namespace {
static size_t GetUniqueIdFromFile(int fd, char* id, size_t max_size) {
  if (max_size < kMaxVarint64Length * 3) {
    return 0;
  }

  struct stat buf;
  int result = fstat(fd, &buf);
  if (result == -1) {
    return 0;
  }

  long version = 0;
  result = ioctl(fd, FS_IOC_GETVERSION, &version);
  if (result == -1) {
    return 0;
  }
  uint64_t uversion = (uint64_t)version;

  char* rid = id;
  rid = EncodeVarint64(rid, buf.st_dev);
  rid = EncodeVarint64(rid, buf.st_ino);
  rid = EncodeVarint64(rid, uversion);
  assert(rid >= id);
  return static_cast<size_t>(rid - id);
#ifdef NOHOST
	/* Get a random number */
#endif
}
}
#endif

/*
 * PosixRandomAccessFile
 *
 * pread() based random-access
 */
PosixRandomAccessFile::PosixRandomAccessFile(const std::string& fname, int fd,
                                             const EnvOptions& options, NoHostFs* nohost)
    : filename_(fname), fd_(fd), use_os_buffer_(options.use_os_buffer), nohost_(nohost) {
  assert(!options.use_mmap_reads || sizeof(void*) < 8);
}

PosixRandomAccessFile::~PosixRandomAccessFile() { nohost_->Close(fd_); }

Status PosixRandomAccessFile::Read(uint64_t offset, size_t n, Slice* result,
                                   char* scratch) const {
  Status s;
  ssize_t r = -1;
  char* ptr = scratch;

  uint64_t origin = nohost_->Lseek(fd_, 0);

  nohost_->Lseek(fd_, offset);
  r = nohost_->SequentialRead(fd_, ptr, n);

  *result = Slice(scratch, (r < 0) ? 0 : r);
  if (r < 0) {
    // An error: return a non-ok status
    s = IOError(filename_, errno);
  }
  if (!use_os_buffer_) {
    // we need to fadvise away the entire range of pages because
    // we do not want readahead pages to be cached.
    Fadvise(fd_, 0, 0, POSIX_FADV_DONTNEED);  // free OS pages
  }
  nohost_->Lseek(fd_, origin);
  return s;
}

#ifdef OS_LINUX
size_t PosixRandomAccessFile::GetUniqueId(char* id, size_t max_size) const {
  return GetUniqueIdFromFile(fd_, id, max_size);
#ifdef NOHOST
	/* Get a random number */
#endif
}
#endif

void PosixRandomAccessFile::Hint(AccessPattern pattern) {
  switch (pattern) {
    case NORMAL:
      Fadvise(fd_, 0, 0, POSIX_FADV_NORMAL);
      break;
    case RANDOM:
      Fadvise(fd_, 0, 0, POSIX_FADV_RANDOM);
      break;
    case SEQUENTIAL:
      Fadvise(fd_, 0, 0, POSIX_FADV_SEQUENTIAL);
      break;
    case WILLNEED:
      Fadvise(fd_, 0, 0, POSIX_FADV_WILLNEED);
      break;
    case DONTNEED:
      Fadvise(fd_, 0, 0, POSIX_FADV_DONTNEED);
      break;
    default:
      assert(false);
      break;
  }
#ifdef NOHOST
	/* Do Nothing -- Ignore */
#endif
}

Status PosixRandomAccessFile::InvalidateCache(size_t offset, size_t length) {
#ifndef OS_LINUX
  return Status::OK();
#else
  // free OS pages
  int ret = Fadvise(fd_, offset, length, POSIX_FADV_DONTNEED);
  if (ret == 0) {
    return Status::OK();
  }
  return IOError(filename_, errno);
#endif
}

/*
 * PosixWritableFile
 *
 * Use posix write to write data to a file.
 */
PosixWritableFile::PosixWritableFile(const std::string& fname, int fd,
                                     const EnvOptions& options, NoHostFs* nohost)
    : filename_(fname), fd_(fd), filesize_(0), nohost_(nohost) {
#ifdef ROCKSDB_FALLOCATE_PRESENT
  allow_fallocate_ = options.allow_fallocate;
  fallocate_with_keep_size_ = options.fallocate_with_keep_size;
#endif
  assert(!options.use_mmap_writes);
}

PosixWritableFile::~PosixWritableFile() {
  if (fd_ >= 0) {
    PosixWritableFile::Close();
  }
}

Status PosixWritableFile::Append(const Slice& data) {
  const char* src = data.data();
  size_t left = data.size();
  while (left != 0) {
    ssize_t done = nohost_->Write(fd_, src, left);
    if (done < 0) {
      if (errno == EINTR) {
        continue;
      }
      return IOError(filename_, errno);
    }
    left -= done;
    src += done;
  }
  filesize_ += data.size();
  return Status::OK();
}

Status PosixWritableFile::Close() {
  Status s;

  size_t block_size;
  size_t last_allocated_block;
  GetPreallocationStatus(&block_size, &last_allocated_block);
  if (last_allocated_block > 0) {
    // trim the extra space preallocated at the end of the file
    // NOTE(ljin): we probably don't want to surface failure as an IOError,
    // but it will be nice to log these errors.
    int dummy __attribute__((unused));
    dummy = ftruncate(fd_, filesize_);
#ifdef ROCKSDB_FALLOCATE_PRESENT
    // in some file systems, ftruncate only trims trailing space if the
    // new file size is smaller than the current size. Calling fallocate
    // with FALLOC_FL_PUNCH_HOLE flag to explicitly release these unused
    // blocks. FALLOC_FL_PUNCH_HOLE is supported on at least the following
    // filesystems:
    //   XFS (since Linux 2.6.38)
    //   ext4 (since Linux 3.0)
    //   Btrfs (since Linux 3.7)
    //   tmpfs (since Linux 3.5)
    // We ignore error since failure of this operation does not affect
    // correctness.
    IOSTATS_TIMER_GUARD(allocate_nanos);
    if (allow_fallocate_) {
      fallocate(fd_, FALLOC_FL_KEEP_SIZE | FALLOC_FL_PUNCH_HOLE, filesize_,
                block_size * last_allocated_block - filesize_);
    }
#endif
  }

  if (nohost_->Close(fd_) < 0) {
    s = IOError(filename_, errno);
  }
  fd_ = -1;
  return s;
}

// write out the cached data to the OS cache
Status PosixWritableFile::Flush() { return Status::OK(); }

Status PosixWritableFile::Sync() {
/*  if (fdatasync(fd_) < 0) {
    return IOError(filename_, errno);
  }*/
  return Status::OK();
}

Status PosixWritableFile::Fsync() {
/*  if (fsync(fd_) < 0) {
    return IOError(filename_, errno);
  }*/
  return Status::OK();
}

bool PosixWritableFile::IsSyncThreadSafe() const { return true; }

uint64_t PosixWritableFile::GetFileSize() { return filesize_; }

Status PosixWritableFile::InvalidateCache(size_t offset, size_t length) {
#ifndef OS_LINUX
  return Status::OK();
#else
  // free OS pages
  int ret = Fadvise(fd_, offset, length, POSIX_FADV_DONTNEED);
  if (ret == 0) {
    return Status::OK();
  }
  return IOError(filename_, errno);
#endif
}

#ifdef ROCKSDB_FALLOCATE_PRESENT
Status PosixWritableFile::Allocate(off_t offset, off_t len) {
  TEST_KILL_RANDOM("PosixWritableFile::Allocate:0", rocksdb_kill_odds);
  IOSTATS_TIMER_GUARD(allocate_nanos);
  int alloc_status = 0;
  if (allow_fallocate_) {
    alloc_status = fallocate(
        fd_, fallocate_with_keep_size_ ? FALLOC_FL_KEEP_SIZE : 0, offset, len);
  }
  if (alloc_status == 0) {
    return Status::OK();
  } else {
    return IOError(filename_, errno);
  }
}

Status PosixWritableFile::RangeSync(off_t offset, off_t nbytes) {
  /*if (sync_file_range(fd_, offset, nbytes, SYNC_FILE_RANGE_WRITE) == 0) {
    return Status::OK();
  } else {
    return IOError(filename_, errno);
  }*/
  return Status::OK();
}

size_t PosixWritableFile::GetUniqueId(char* id, size_t max_size) const {
  //return GetUniqueIdFromFile(fd_, id, max_size);
  return 0;
}
#endif

PosixDirectory::~PosixDirectory() { nohost_->Close(fd_); }

Status PosixDirectory::Fsync() {
/*  if (fsync(fd_) == -1) {
    return IOError("directory", errno);
  }*/
  return Status::OK();
}
}  // namespace rocksdb
//#endif

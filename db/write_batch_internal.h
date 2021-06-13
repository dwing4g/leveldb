// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_WRITE_BATCH_INTERNAL_H_
#define STORAGE_LEVELDB_DB_WRITE_BATCH_INTERNAL_H_

#include "db/dbformat.h"
#include "leveldb/write_batch.h"

namespace leveldb {

class MemTable;

// WriteBatchInternal provides static methods for manipulating a
// WriteBatch that we don't want in the public WriteBatch interface.
class WriteBatchInternal {
 public:
  // Return the number of entries in the batch.
  static int Count(const WriteBatch* batch);

  // Set the count for the number of entries in the batch.
  static void SetCount(WriteBatch* batch, int n);

  // Return the sequence number for the start of this batch.
  static SequenceNumber Sequence(const WriteBatch* batch);

  // Store the specified number as the sequence number for the start of
  // this batch.
  static void SetSequence(WriteBatch* batch, SequenceNumber seq);

  static Slice Contents(const WriteBatch* batch) { return Slice(batch->buf_, batch->len_); }

  static size_t ByteSize(const WriteBatch* batch) { return batch->len_; }

  static void SetContents(WriteBatch* batch, const Slice& contents);

  static Status InsertInto(const WriteBatch* batch, MemTable* memtable);

  static void Append(WriteBatch* dst, const WriteBatch* src);

  static void EnsureCapacity(WriteBatch* batch, size_t cap) {
    if (batch->cap_ < cap) {
      if (batch->cap_ == 0)
        batch->cap_ = cap;
      else {
        do batch->cap_ <<= 1;
        while (batch->cap_ < cap);
      }
      batch->buf_ = (char*)realloc(batch->buf_, batch->cap_);
    }
  }

  static void Append(WriteBatch* batch, const char* buf, size_t size) {
    size_t cap_need = batch->len_ + size;
    EnsureCapacity(batch, cap_need);
    memcpy(batch->buf_ + batch->len_, buf, size);
    batch->len_ = cap_need;
  }

  static char* Resize(WriteBatch* batch, size_t size) {
    if (size > batch->cap_)
      batch->buf_ = (char*)realloc(batch->buf_, batch->cap_ = size);
    batch->len_ = size;
    return batch->buf_;
  }
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_WRITE_BATCH_INTERNAL_H_

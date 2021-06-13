// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// WriteBatch::rep_ :=
//    sequence: fixed64
//    count: fixed32
//    data: record[count]
// record :=
//    kTypeValue varstring varstring         |
//    kTypeDeletion varstring
// varstring :=
//    len: varint32
//    data: uint8[len]

#include <algorithm>

#include "leveldb/write_batch.h"

#include "db/dbformat.h"
#include "db/memtable.h"
#include "db/write_batch_internal.h"
#include "leveldb/db.h"
#include "util/coding.h"

namespace leveldb {

// WriteBatch header has an 8-byte sequence number followed by a 4-byte count.
static const size_t kHeader = 12;

WriteBatch::WriteBatch() : buf_(0), len_(0), cap_(0) { Clear(); }

WriteBatch::~WriteBatch() { free(buf_); }

WriteBatch::Handler::~Handler() = default;

void WriteBatch::Clear() {
  WriteBatchInternal::EnsureCapacity(this, kHeader + 124); // initial capacity for benchmark (16B key, 100B value)
  WriteBatchInternal::Resize(this, kHeader);
  memset(buf_, 0, kHeader);
}

size_t WriteBatch::ApproximateSize() const { return len_; }

Status WriteBatch::Iterate(Handler* handler) const {
  Slice input(buf_, len_);
  if (input.size() < kHeader) {
    return Status::Corruption("malformed WriteBatch (too small)");
  }

  input.remove_prefix(kHeader);
  Slice key, value;
  int found = 0;
  while (!input.empty()) {
    found++;
    char tag = input[0];
    input.remove_prefix(1);
    switch (tag) {
      case kTypeValue:
        if (GetLengthPrefixedSlice(&input, &key) &&
            GetLengthPrefixedSlice(&input, &value)) {
          handler->Put(key, value);
        } else {
          return Status::Corruption("bad WriteBatch Put");
        }
        break;
      case kTypeDeletion:
        if (GetLengthPrefixedSlice(&input, &key)) {
          handler->Delete(key);
        } else {
          return Status::Corruption("bad WriteBatch Delete");
        }
        break;
      default:
        return Status::Corruption("unknown WriteBatch tag");
    }
  }
  if (found != WriteBatchInternal::Count(this)) {
    return Status::Corruption("WriteBatch has wrong count");
  } else {
    return Status::OK();
  }
}

int WriteBatchInternal::Count(const WriteBatch* b) {
  return DecodeFixed32(b->buf_ + 8);
}

void WriteBatchInternal::SetCount(WriteBatch* b, int n) {
  EncodeFixed32(b->buf_ + 8, n);
}

SequenceNumber WriteBatchInternal::Sequence(const WriteBatch* b) {
  return SequenceNumber(DecodeFixed64(b->buf_));
}

void WriteBatchInternal::SetSequence(WriteBatch* b, SequenceNumber seq) {
  EncodeFixed64(b->buf_, seq);
}

void WriteBatch::Put(const Slice& key, const Slice& value) {
  WriteBatchInternal::SetCount(this, WriteBatchInternal::Count(this) + 1);
  const uint32_t klen = static_cast<uint32_t>(std::min(key.size(), static_cast<size_t>(UINT32_MAX)));
  const uint32_t vlen = static_cast<uint32_t>(std::min(value.size(), static_cast<size_t>(UINT32_MAX)));
  WriteBatchInternal::EnsureCapacity(this, len_ + 1 + 5 + klen + 5 + vlen);
  char* p = buf_ + len_;
  *p++ = static_cast<char>(kTypeValue);
  p = EncodeVarint32(p, klen);
  memcpy(p, key.data(), klen);
  p = EncodeVarint32(p + klen, vlen);
  memcpy(p, value.data(), vlen);
  len_ = p + vlen - buf_;
}

void WriteBatch::Delete(const Slice& key) {
  WriteBatchInternal::SetCount(this, WriteBatchInternal::Count(this) + 1);
  const uint32_t klen = static_cast<uint32_t>(std::min(key.size(), static_cast<size_t>(UINT32_MAX)));
  WriteBatchInternal::EnsureCapacity(this, len_ + 1 + 5 + klen);
  char* p = buf_ + len_;
  *p++ = static_cast<char>(kTypeDeletion);
  p = EncodeVarint32(p, klen);
  memcpy(p, key.data(), klen);
  len_ = p + klen - buf_;
}

void WriteBatch::Append(const WriteBatch& source) {
  WriteBatchInternal::Append(this, &source);
}

namespace {
class MemTableInserter : public WriteBatch::Handler {
 public:
  SequenceNumber sequence_;
  MemTable* mem_;

  void Put(const Slice& key, const Slice& value) override {
    mem_->Add(sequence_, kTypeValue, key, value);
    sequence_++;
  }
  void Delete(const Slice& key) override {
    mem_->Add(sequence_, kTypeDeletion, key, Slice());
    sequence_++;
  }
};
}  // namespace

Status WriteBatchInternal::InsertInto(const WriteBatch* b, MemTable* memtable) {
  MemTableInserter inserter;
  inserter.sequence_ = WriteBatchInternal::Sequence(b);
  inserter.mem_ = memtable;
  return b->Iterate(&inserter);
}

void WriteBatchInternal::SetContents(WriteBatch* b, const Slice& contents) {
  assert(contents.size() >= kHeader);
  if (b->cap_ < contents.size()) {
    free(b->buf_);
    b->buf_ = (char*)malloc(b->cap_ = contents.size());
  }
  memcpy(b->buf_, contents.data(), b->len_ = contents.size());
}

void WriteBatchInternal::Append(WriteBatch* dst, const WriteBatch* src) {
  SetCount(dst, Count(dst) + Count(src));
  assert(src->len_ >= kHeader);
  Append(dst, src->buf_ + kHeader, src->len_ - kHeader);
}

}  // namespace leveldb

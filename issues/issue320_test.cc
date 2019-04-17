// Copyright (c) 2019 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "leveldb/db.h"
#include "leveldb/write_batch.h"
#include "util/testharness.h"

namespace leveldb {

namespace {

// Creates a random number in the range of [0, max).
int GenerateRandomNumber(int max) { return std::rand() % max; }

std::string CreateRandomString(int32_t index) {
  static const size_t len = 1024;
  char bytes[len];
  size_t i = 0;
  while (i < 8) {
    bytes[i] = 'a' + ((index >> (4 * i)) & 0xf);
    ++i;
  }
  while (i < sizeof(bytes)) {
    bytes[i] = 'a' + GenerateRandomNumber(26);
    ++i;
  }
  return std::string(bytes, sizeof(bytes));
}

}  // namespace

class Issue320 {};

TEST(Issue320, Test) {
  std::srand(0);

  bool delete_before_put = false;
  bool keep_snapshots = true;

  std::vector<std::pair<std::string, std::string>*> test_map(10000, (std::pair<std::string, std::string>*)0);
  std::vector<Snapshot const*> snapshots(100, (Snapshot const*)0);

  DB* db;
  Options options;
  options.create_if_missing = true;

  std::string dbpath = test::TmpDir() + "/leveldb_issue320_test";
  ASSERT_OK(DB::Open(options, dbpath, &db));

  uint32_t target_size = 10000;
  uint32_t num_items = 0;
  uint32_t count = 0;
  std::string key;
  std::string value, old_value;

  WriteOptions writeOptions;
  ReadOptions readOptions;
  while (count < 200000) {
    if ((++count % 1000) == 0) {
      std::cout << "count: " << count << std::endl;
    }

    int index = GenerateRandomNumber(test_map.size());
    WriteBatch batch;

    if (test_map[index] == 0) {
      num_items++;
      test_map[index] = new std::pair<std::string, std::string>(
          CreateRandomString(index), CreateRandomString(index));
      batch.Put(test_map[index]->first, test_map[index]->second);
    } else {
      ASSERT_OK(db->Get(readOptions, test_map[index]->first, &old_value));
      if (old_value != test_map[index]->second) {
        std::cout << "ERROR incorrect value returned by Get" << std::endl;
        std::cout << "  count=" << count << std::endl;
        std::cout << "  old value=" << old_value << std::endl;
        std::cout << "  test_map[index]->second=" << test_map[index]->second << std::endl;
        std::cout << "  test_map[index]->first=" << test_map[index]->first << std::endl;
        std::cout << "  index=" << index << std::endl;
        ASSERT_EQ(old_value, test_map[index]->second);
      }

      if (num_items >= target_size && GenerateRandomNumber(100) > 30) {
        batch.Delete(test_map[index]->first);
        delete test_map[index];
        test_map[index] = 0;
        --num_items;
      } else {
        test_map[index]->second = CreateRandomString(index);
        if (delete_before_put) batch.Delete(test_map[index]->first);
        batch.Put(test_map[index]->first, test_map[index]->second);
      }
    }

    ASSERT_OK(db->Write(writeOptions, &batch));

    if (keep_snapshots && GenerateRandomNumber(10) == 0) {
      int i = GenerateRandomNumber(snapshots.size());
      if (snapshots[i] != 0) {
        db->ReleaseSnapshot(snapshots[i]);
      }
      snapshots[i] = db->GetSnapshot();
    }
  }

  for (std::vector<Snapshot const*>::const_iterator it = snapshots.begin(), ie = snapshots.end(); it != ie; ++it) {
  	Snapshot const* snapshot = *it;
    if (snapshot) {
      db->ReleaseSnapshot(snapshot);
    }
  }

  for (size_t i = 0; i < test_map.size(); ++i) {
    if (test_map[i] != 0) {
      delete test_map[i];
      test_map[i] = 0;
    }
  }

  delete db;
  DestroyDB(dbpath, options);
}

}  // namespace leveldb

int main(int argc, char** argv) { return leveldb::test::RunAllTests(); }

#!/bin/sh

cd `dirname $0`

if [ "$JAVA_HOME" = "" ]; then JAVA_HOME=/usr/java/default; fi

CORE_FILES="db/builder.cc db/c.cc db/db_impl.cc db/db_iter.cc db/dbformat.cc db/filename.cc db/log_reader.cc db/log_writer.cc db/memtable.cc db/repair.cc db/table_cache.cc db/version_edit.cc db/version_set.cc db/write_batch.cc table/block.cc table/block_builder.cc table/filter_block.cc table/format.cc table/iterator.cc table/merger.cc table/table.cc table/table_builder.cc table/two_level_iterator.cc util/arena.cc util/bloom.cc util/cache.cc util/coding.cc util/comparator.cc util/crc32c.cc util/env.cc util/env_posix.cc util/env_windows.cc util/filter_policy.cc util/hash.cc util/histogram.cc util/jni.cc util/logging.cc util/options.cc util/status.cc port/port_posix.cc snappy/snappy.cc snappy/snappy-sinksource.cc snappy/snappy-stubs-internal.cc"

TEST_FILES="util/testutil.cc util/testharness.cc"

OBJ_FILES="builder.o c.o db_impl.o db_iter.o dbformat.o filename.o log_reader.o log_writer.o memtable.o repair.o table_cache.o version_edit.o version_set.o write_batch.o block.o block_builder.o filter_block.o format.o iterator.o merger.o table.o table_builder.o two_level_iterator.o arena.o bloom.o cache.o coding.o comparator.o crc32c.o env.o env_posix.o env_windows.o filter_policy.o hash.o histogram.o jni.o logging.o options.o status.o testutil.o testharness.o port_posix.o snappy.o snappy-sinksource.o snappy-stubs-internal.o"

COMPILE="g++ -DOS_LINUX -DLEVELDB_PLATFORM_POSIX -DSNAPPY -DENABLE_JNI -DNDEBUG -I. -Iinclude -I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux -O3 -ffast-math -fweb -fomit-frame-pointer -fmerge-all-constants -fno-builtin-memcmp -fPIC -pipe -pthread -s"

$COMPILE -shared -Wl,-soname -Wl,libleveldbjni.so -o libleveldbjni.so $CORE_FILES

$COMPILE -c $CORE_FILES $TEST_FILES
ar -rs libleveldb.a $OBJ_FILES
rm -f *.o 2> /dev/null

$COMPILE -o db_bench     db/db_bench.cc     libleveldb.a
$COMPILE -o db_test      db/db_test.cc      libleveldb.a
$COMPILE -o leveldb_main db/leveldb_main.cc libleveldb.a

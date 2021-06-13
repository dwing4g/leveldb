#!/bin/sh

cd `dirname $0`

# install JDK 8+ and set $JAVA_INCLUDE if JDK is not in default path

if [ "$JAVA_INCLUDE" = "" ]; then JAVA_INCLUDE=/System/Library/Frameworks/JavaVM.framework/Versions/A/Headers; fi

CORE_FILES="\
db/builder.cc \
db/db_impl.cc \
db/db_iter.cc \
db/dbformat.cc \
db/dumpfile.cc \
db/filename.cc \
db/log_reader.cc \
db/log_writer.cc \
db/memtable.cc \
db/repair.cc \
db/table_cache.cc \
db/version_edit.cc \
db/version_set.cc \
db/write_batch.cc \
table/block.cc \
table/block_builder.cc \
table/filter_block.cc \
table/format.cc \
table/iterator.cc \
table/merger.cc \
table/table.cc \
table/table_builder.cc \
table/two_level_iterator.cc \
util/arena.cc \
util/bloom.cc \
util/cache.cc \
util/coding.cc \
util/comparator.cc \
util/crc32c.cc \
util/env.cc \
util/env_posix.cc \
util/filter_policy.cc \
util/hash.cc \
util/histogram.cc \
util/logging.cc \
util/options.cc \
util/status.cc \
crc32c/crc32c_portable.cc \
snappy/snappy.cc \
snappy/snappy-sinksource.cc \
snappy/snappy-stubs-internal.cc \
"

OBJ_FILES="\
builder.o \
db_impl.o \
db_iter.o \
dbformat.o \
dumpfile.o \
filename.o \
log_reader.o \
log_writer.o \
memtable.o \
repair.o \
table_cache.o \
version_edit.o \
version_set.o \
write_batch.o \
block.o \
block_builder.o \
filter_block.o \
format.o \
iterator.o \
merger.o \
table.o \
table_builder.o \
two_level_iterator.o \
arena.o \
bloom.o \
cache.o \
coding.o \
comparator.o \
crc32c.o \
env.o \
env_posix.o \
filter_policy.o \
hash.o \
histogram.o \
logging.o \
options.o \
status.o \
crc32c_portable.o \
snappy.o \
snappy-sinksource.o \
snappy-stubs-internal.o \
"

BENCHMARK_FILES="\
benchmark/benchmark.cc \
benchmark/benchmark_api_internal.cc \
benchmark/benchmark_name.cc \
benchmark/benchmark_register.cc \
benchmark/benchmark_runner.cc \
benchmark/colorprint.cc \
benchmark/commandlineflags.cc \
benchmark/complexity.cc \
benchmark/console_reporter.cc \
benchmark/counter.cc \
benchmark/csv_reporter.cc \
benchmark/json_reporter.cc \
benchmark/perf_counters.cc \
benchmark/reporter.cc \
benchmark/sleep.cc \
benchmark/statistics.cc \
benchmark/string_util.cc \
benchmark/sysinfo.cc \
benchmark/timers.cc \
"

COMPILE="g++ -std=c++11 -mmacosx-version-min=10.9 -DNDEBUG -DLEVELDB_PLATFORM_POSIX -DHAVE_CRC32C=1 -DHAVE_SNAPPY=1 -DHAVE_BUILTIN_EXPECT=1 -DHAVE_BYTESWAP_H=1 -DHAVE_BUILTIN_CTZ=1 -DHAVE_FULLFSYNC=1 -DHAVE_O_CLOEXEC=1 -I. -Iinclude -Isnappy -Igtest -Igmock -I${JAVA_INCLUDE} -m64 -mavx -mbmi2 -O3 -fno-strict-aliasing -fwrapv -fomit-frame-pointer -fmerge-all-constants -fno-builtin-memcmp -pipe -pthread"

echo building libleveldbjni64.dylib ...
$COMPILE -c                -o crc32c_.o      crc32c/crc32c.cc
$COMPILE -c -msse4.2 -fPIC -o crc32c_sse42.o crc32c/crc32c_sse42.cc
$COMPILE -DLEVELDB_SHARED_LIBRARY -DLEVELDB_COMPILE_LIBRARY -DENABLE_JNI -shared -fPIC -fvisibility=hidden -dynamiclib -o libleveldbjni64.dylib $CORE_FILES util/jni.cc crc32c/crc32c.cc crc32c_sse42.o

echo building libleveldb.a ...
$COMPILE -c $CORE_FILES
ar -rs libleveldb.a $OBJ_FILES crc32c_.o crc32c_sse42.o
rm -f *.o 2> /dev/null

echo building db-tools ...
$COMPILE -o leveldbutil      db/leveldbutil.cc      -lpthread libleveldb.a
$COMPILE -o db_bench         benchmarks/db_bench.cc -lpthread libleveldb.a util/testutil.cc
$COMPILE -o db_test          db/db_test.cc          -lpthread libleveldb.a util/testutil.cc gtest/src/gtest-all.cc gmock/src/gmock-all.cc $BENCHMARK_FILES
$COMPILE -o env_test         util/env_test.cc       -lpthread libleveldb.a util/testutil.cc gtest/src/gtest-all.cc gmock/src/gmock-all.cc
$COMPILE -o version_set_test db/version_set_test.cc -lpthread libleveldb.a util/testutil.cc gtest/src/gtest-all.cc gmock/src/gmock-all.cc

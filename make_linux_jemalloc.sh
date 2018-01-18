#!/bin/sh

cd `dirname $0`

# note:
# 1. if gcc supports c++11 (4.8+), remove "-Dconstexpr= -Doverride="
# 2. configure and make jemalloc, then put the result "lib/libjemalloc.a" and "lib/libjemalloc_pic.a" in this path

if [ "$JAVA_HOME" = "" ]; then JAVA_HOME=/usr/java/default; fi

CORE_FILES="\
db/builder.cc \
db/c.cc \
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
util/jni.cc \
util/logging.cc \
util/options.cc \
util/status.cc \
port/port_posix.cc \
crc32c/crc32c_portable.cc \
snappy/snappy.cc \
snappy/snappy-sinksource.cc \
snappy/snappy-stubs-internal.cc \
"

TEST_FILES="\
util/testutil.cc \
util/testharness.cc \
"

OBJ_FILES="\
builder.o \
c.o \
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
jni.o \
logging.o \
options.o \
status.o \
port_posix.o \
crc32c_.o \
crc32c_portable.o \
crc32c_sse42.o \
snappy.o \
snappy-sinksource.o \
snappy-stubs-internal.o \
testutil.o \
testharness.o \
"

COMPILE="g++ -std=c++0x -DOS_LINUX=1 -DLEVELDB_PLATFORM_POSIX=1 -DHAVE_CRC32C=1 -DHAVE_SNAPPY=1 -DHAVE_BUILTIN_EXPECT=1 -DHAVE_BYTESWAP_H=1 -DHAVE_BUILTIN_CTZ=1 -DENABLE_JNI -DNDEBUG -Dconstexpr= -Doverride= -I. -Iinclude -Isnappy -I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux -m64 -O3 -ffast-math -fweb -fomit-frame-pointer -fmerge-all-constants -fno-builtin-memcmp -fPIC -pipe -pthread -ldl"

$COMPILE -c          -o crc32c_.o      crc32c/crc32c.cc
$COMPILE -c -msse4.2 -o crc32c_sse42.o crc32c/crc32c_sse42.cc

echo building libleveldbjni64.so ...
$COMPILE -DLEVELDB_SHARED_LIBRARY=1 -DLEVELDB_COMPILE_LIBRARY=1 -shared -fvisibility=hidden -Wl,-soname -Wl,libleveldbjni64.so -o libleveldbjni64.so $CORE_FILES crc32c/crc32c.cc crc32c_sse42.o libjemalloc_pic.a

echo building libleveldb.a ...
$COMPILE -c $CORE_FILES $TEST_FILES
ar -rs libleveldb.a $OBJ_FILES
rm -f *.o 2> /dev/null

echo building db-tools ...
$COMPILE      -o leveldbutil db/leveldbutil.cc libleveldb.a libjemalloc.a
$COMPILE -lrt -o db_bench    db/db_bench.cc    libleveldb.a libjemalloc.a
$COMPILE -lrt -o db_test     db/db_test.cc     libleveldb.a libjemalloc.a
$COMPILE      -o env_test    util/env_test.cc  libleveldb.a libjemalloc.a

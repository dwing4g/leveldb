@echo off
setlocal
pushd %~dp0

rem install mingw-gcc 4.8+ and append PATH with mingw/bin

set CORE_FILES=^
db/builder.cc ^
db/c.cc ^
db/db_impl.cc ^
db/db_iter.cc ^
db/dbformat.cc ^
db/dumpfile.cc ^
db/filename.cc ^
db/log_reader.cc ^
db/log_writer.cc ^
db/memtable.cc ^
db/repair.cc ^
db/table_cache.cc ^
db/version_edit.cc ^
db/version_set.cc ^
db/write_batch.cc ^
table/block.cc ^
table/block_builder.cc ^
table/filter_block.cc ^
table/format.cc ^
table/iterator.cc ^
table/merger.cc ^
table/table.cc ^
table/table_builder.cc ^
table/two_level_iterator.cc ^
util/arena.cc ^
util/bloom.cc ^
util/cache.cc ^
util/coding.cc ^
util/comparator.cc ^
util/crc32c.cc ^
util/env.cc ^
util/env_windows.cc ^
util/filter_policy.cc ^
util/hash.cc ^
util/histogram.cc ^
util/jni.cc ^
util/logging.cc ^
util/options.cc ^
util/status.cc ^
port/port_posix.cc ^
snappy/snappy.cc ^
snappy/snappy-sinksource.cc ^
snappy/snappy-stubs-internal.cc

set TEST_FILES=^
util/testutil.cc ^
util/testharness.cc

set OBJ_FILES=^
builder.o ^
c.o ^
db_impl.o ^
db_iter.o ^
dbformat.o ^
dumpfile.o ^
filename.o ^
log_reader.o ^
log_writer.o ^
memtable.o ^
repair.o ^
table_cache.o ^
version_edit.o ^
version_set.o ^
write_batch.o ^
block.o ^
block_builder.o ^
filter_block.o ^
format.o ^
iterator.o ^
merger.o ^
table.o ^
table_builder.o ^
two_level_iterator.o ^
arena.o ^
bloom.o ^
cache.o ^
coding.o ^
comparator.o ^
crc32c.o ^
env.o ^
env_windows.o ^
filter_policy.o ^
hash.o ^
histogram.o ^
jni.o ^
logging.o ^
options.o ^
status.o ^
port_posix.o ^
snappy.o ^
snappy-sinksource.o ^
snappy-stubs-internal.o ^
testutil.o ^
testharness.o

set COMPILE=-DOS_WIN -DLEVELDB_PLATFORM_POSIX -DLEVELDB_PLATFORM_POSIX_SSE -DSNAPPY -DENABLE_JNI -D_POSIX -D__USE_MINGW_ANSI_STDIO=1 -DNDEBUG -I. -Iinclude -Isnappy -Iport -Iport/vs/jni -Ofast -ffast-math -fweb -fomit-frame-pointer -fmerge-all-constants -fno-builtin-memcmp -pipe -pthread -static -lpthread

set COMPILE32=i686-w64-mingw32-g++.exe -m32 -march=i686 -flto -fwhole-program %COMPILE%
set COMPILE64=x86_64-w64-mingw32-g++.exe -m64 %COMPILE%

%COMPILE32% -c -msse4.2 -o port_posix_sse_32.o port/port_posix_sse.cc
%COMPILE64% -c -msse4.2 -o port_posix_sse_64.o port/port_posix_sse.cc

echo building leveldbjni32.dll ...
%COMPILE32% -shared -Wl,--image-base,0x10000000 -Wl,--kill-at -Wl,-soname -Wl,leveldbjni32.dll -o leveldbjni32.dll %CORE_FILES% port_posix_sse_32.o

echo building leveldbjni64.dll ...
%COMPILE64% -shared -Wl,--image-base,0x10000000 -Wl,--kill-at -Wl,-soname -Wl,leveldbjni64.dll -o leveldbjni64.dll %CORE_FILES% port_posix_sse_64.o

echo building db-tools ...
%COMPILE32% -o leveldbutil32.exe db/leveldbutil.cc %CORE_FILES% %TEST_FILES% port_posix_sse_32.o
%COMPILE64% -o leveldbutil64.exe db/leveldbutil.cc %CORE_FILES% %TEST_FILES% port_posix_sse_64.o
%COMPILE32% -o db_bench32.exe    db/db_bench.cc    %CORE_FILES% %TEST_FILES% port_posix_sse_32.o
%COMPILE64% -o db_bench64.exe    db/db_bench.cc    %CORE_FILES% %TEST_FILES% port_posix_sse_64.o
%COMPILE32% -o db_test32.exe     db/db_test.cc     %CORE_FILES% %TEST_FILES% port_posix_sse_32.o
%COMPILE64% -o db_test64.exe     db/db_test.cc     %CORE_FILES% %TEST_FILES% port_posix_sse_64.o
%COMPILE32% -o env_test32.exe    util/env_test.cc  %CORE_FILES% %TEST_FILES% port_posix_sse_32.o
%COMPILE64% -o env_test64.exe    util/env_test.cc  %CORE_FILES% %TEST_FILES% port_posix_sse_64.o

del port_posix_sse_??.o

pause

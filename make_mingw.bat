@echo off
setlocal
pushd %~dp0

rem require mingw-gcc 4.8+ (c++11) with posix thread; 5.0+ for db-tools
set path=C:\TDM-GCC-64\bin;%path%

set CORE_FILES=^
db/builder.cc ^
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
util/logging.cc ^
util/options.cc ^
util/status.cc ^
crc32c/crc32c.cc ^
crc32c/crc32c_portable.cc ^
snappy/snappy.cc ^
snappy/snappy-sinksource.cc ^
snappy/snappy-stubs-internal.cc

set BENCHMARK_FILES=^
benchmark/benchmark.cc ^
benchmark/benchmark_api_internal.cc ^
benchmark/benchmark_name.cc ^
benchmark/benchmark_register.cc ^
benchmark/benchmark_runner.cc ^
benchmark/colorprint.cc ^
benchmark/commandlineflags.cc ^
benchmark/complexity.cc ^
benchmark/console_reporter.cc ^
benchmark/counter.cc ^
benchmark/csv_reporter.cc ^
benchmark/json_reporter.cc ^
benchmark/perf_counters.cc ^
benchmark/reporter.cc ^
benchmark/sleep.cc ^
benchmark/statistics.cc ^
benchmark/string_util.cc ^
benchmark/sysinfo.cc ^
benchmark/timers.cc

set COMPILE=-std=c++11 -DNDEBUG -DLEVELDB_PLATFORM_WINDOWS -DHAVE_CRC32C=1 -DHAVE_SNAPPY=1 -DHAVE_BUILTIN_EXPECT=1 -DHAVE_BYTESWAP_H=1 -DHAVE_BUILTIN_CTZ=1 -D__USE_MINGW_ANSI_STDIO=1 -I. -Iinclude -Iport/win -Isnappy -Igtest -Igmock -O3 -fweb -fno-strict-aliasing -fwrapv -fomit-frame-pointer -fmerge-all-constants -fno-builtin-memcmp -flto -fwhole-program -Wno-attributes -static -pipe

set COMPILE32=g++.exe -m32 -march=i686 %COMPILE% -Wl,--enable-stdcall-fixup
set COMPILE64=x86_64-w64-mingw32-g++.exe -m64 %COMPILE%

%COMPILE32% -c -msse4.2 -o crc32c_sse42_32.o crc32c/crc32c_sse42.cc
%COMPILE64% -c -msse4.2 -o crc32c_sse42_64.o crc32c/crc32c_sse42.cc

echo building leveldbjni32.dll ...
%COMPILE32% -DLEVELDB_SHARED_LIBRARY -DLEVELDB_COMPILE_LIBRARY -DLEVELDB_EXPORT= -DENABLE_JNI -shared -Wl,--image-base,0x10000000 -Wl,--kill-at -Wl,-soname -Wl,leveldbjni32.dll -o leveldbjni32.dll %CORE_FILES% util/jni.cc crc32c_sse42_32.o

echo building leveldbjni64.dll ...
%COMPILE64% -DLEVELDB_SHARED_LIBRARY -DLEVELDB_COMPILE_LIBRARY -DLEVELDB_EXPORT= -DENABLE_JNI -shared -Wl,--image-base,0x10000000 -Wl,--kill-at -Wl,-soname -Wl,leveldbjni64.dll -o leveldbjni64.dll %CORE_FILES% util/jni.cc crc32c_sse42_64.o

echo building db-tools ...
%COMPILE32% -o leveldbutil32.exe       db/leveldbutil.cc      %CORE_FILES% crc32c_sse42_32.o
%COMPILE64% -o leveldbutil64.exe       db/leveldbutil.cc      %CORE_FILES% crc32c_sse42_64.o
%COMPILE32% -o db_bench32.exe          benchmarks/db_bench.cc %CORE_FILES% crc32c_sse42_32.o util/testutil.cc
%COMPILE64% -o db_bench64.exe          benchmarks/db_bench.cc %CORE_FILES% crc32c_sse42_64.o util/testutil.cc
%COMPILE32% -o db_test32.exe           db/db_test.cc          %CORE_FILES% crc32c_sse42_32.o util/testutil.cc gtest/src/gtest-all.cc gmock/src/gmock-all.cc %BENCHMARK_FILES% -lshlwapi
%COMPILE64% -o db_test64.exe           db/db_test.cc          %CORE_FILES% crc32c_sse42_64.o util/testutil.cc gtest/src/gtest-all.cc gmock/src/gmock-all.cc %BENCHMARK_FILES% -lshlwapi
%COMPILE32% -o env_test32.exe          util/env_test.cc       %CORE_FILES% crc32c_sse42_32.o util/testutil.cc gtest/src/gtest-all.cc gmock/src/gmock-all.cc
%COMPILE64% -o env_test64.exe          util/env_test.cc       %CORE_FILES% crc32c_sse42_64.o util/testutil.cc gtest/src/gtest-all.cc gmock/src/gmock-all.cc
%COMPILE32% -o version_set_test32.exe  db/version_set_test.cc %CORE_FILES% crc32c_sse42_32.o util/testutil.cc gtest/src/gtest-all.cc gmock/src/gmock-all.cc
%COMPILE64% -o version_set_test64.exe  db/version_set_test.cc %CORE_FILES% crc32c_sse42_64.o util/testutil.cc gtest/src/gtest-all.cc gmock/src/gmock-all.cc

del crc32c_sse42_??.o

pause

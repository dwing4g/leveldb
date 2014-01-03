@echo off
setlocal
pushd %~dp0

rem install mingw-gcc 4.8+ and append PATH with mingw/bin
rem install jdk6+ and set JAVA_HOME
if "%JAVA_HOME%" equ "" set JAVA_HOME=C:/Program Files/Java/jdk1.7.0_21

set CPPFILES=db/builder.cc db/c.cc db/db_impl.cc db/db_iter.cc db/dbformat.cc db/filename.cc db/log_reader.cc db/log_writer.cc db/memtable.cc db/repair.cc db/table_cache.cc db/version_edit.cc db/version_set.cc db/write_batch.cc table/block.cc table/block_builder.cc table/filter_block.cc table/format.cc table/iterator.cc table/merger.cc table/table.cc table/table_builder.cc table/two_level_iterator.cc util/arena.cc util/bloom.cc util/cache.cc util/coding.cc util/comparator.cc util/crc32c.cc util/env.cc util/env_posix.cc util/env_windows.cc util/filter_policy.cc util/hash.cc util/histogram.cc util/jni.cc util/logging.cc util/options.cc util/status.cc util/testutil.cc util/testharness.cc port/port_posix.cc

set OBJFILES=builder.o c.o db_impl.o db_iter.o dbformat.o filename.o log_reader.o log_writer.o memtable.o repair.o table_cache.o version_edit.o version_set.o write_batch.o block.o block_builder.o filter_block.o format.o iterator.o merger.o table.o table_builder.o two_level_iterator.o arena.o bloom.o cache.o coding.o comparator.o crc32c.o env.o env_posix.o env_windows.o filter_policy.o hash.o histogram.o jni.o logging.o options.o status.o testutil.o testharness.o port_posix.o

set COMPILE=g++ -DOS_LINUX -DOS_MINGW -DLEVELDB_PLATFORM_POSIX -DNDEBUG -DENABLE_JNI -I. -I./include -I"%JAVA_HOME%/include" -I"%JAVA_HOME%/include/win64" -I"%JAVA_HOME%/include/win32" -march=i686 -Ofast -flto -fwhole-program -ffast-math -fweb -fomit-frame-pointer -fmerge-all-constants -fno-builtin-memcmp -pipe -pthread -static -s -lpthread

%COMPILE% -shared -Wl,--image-base,0x10000000 -Wl,--kill-at -Wl,-soname -Wl,leveldbjni.dll -o leveldbjni.dll %CPPFILES%

%COMPILE% -c %CPPFILES%
ar -rs libleveldb.a %OBJFILES%
del *.o 2>nul

%COMPILE% -o db_bench.exe     db/db_bench.cc     libleveldb.a
%COMPILE% -o db_test.exe      db/db_test.cc      libleveldb.a
%COMPILE% -o leveldb_main.exe db/leveldb_main.cc libleveldb.a

pause
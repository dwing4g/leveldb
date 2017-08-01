// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifdef _WIN32

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x500
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#undef DeleteFile
#undef min
#include "leveldb/env.h"
#include "leveldb/status.h"
#include "port/port.h"
#define localtime_r(x,y) localtime_s(y,x)
#include "util/posix_logger.h"

namespace leveldb {

namespace {

struct ThreadParam {
	void (*function)(void* arg);
	void* arg;
	ThreadParam(void (*function)(void* arg), void* arg) : function(function), arg(arg) {}
};

static DWORD WINAPI ThreadProc(LPVOID lpParameter) {
	ThreadParam* param = static_cast<ThreadParam*>(lpParameter);
	void (*function)(void* arg) = param->function;
	void* arg = param->arg;
	delete param;
	function(arg);
	return 0;
}

static WCHAR* Utf8_Wchar(const std::string& src, WCHAR dst[MAX_PATH]) {
	dst[MultiByteToWideChar(65001, 0, static_cast<LPCSTR>(src.c_str()), (int)src.size(), dst, MAX_PATH - 1)] = 0;
	return dst;
}

static char* Wchar_Utf8(const WCHAR* src, char dst[MAX_PATH]) {
	dst[WideCharToMultiByte(65001, 0, static_cast<LPCWSTR>(src), -1, dst, MAX_PATH - 1, 0, 0)] = 0;
	return dst;
}

class WindowsSequentialFile : public SequentialFile {
	std::string fname_;
	HANDLE file_;

public:
	WindowsSequentialFile(const std::string& fname, HANDLE file) : fname_(fname), file_(file) {}
	virtual ~WindowsSequentialFile() { CloseHandle(file_); }

	virtual Status Read(size_t n, Slice* result, char* scratch) {
		DWORD bytesRead = 0;
		BOOL success = ReadFile(file_, scratch, (DWORD)n, &bytesRead, 0);
		*result = Slice(scratch, bytesRead);
		return success ? Status::OK() : Status::IOError(fname_);
	}

	virtual Status Skip(uint64_t n) {
		LARGE_INTEGER li;
		li.QuadPart = n;
		return SetFilePointerEx(file_, li, 0, FILE_CURRENT) ? Status::OK() : Status::IOError(fname_);
	}
};

// A file abstraction for randomly reading the contents of a file.
class WindowsRandomAccessFile : public RandomAccessFile {
	std::string fname_;
	HANDLE file_;

public:
	WindowsRandomAccessFile(const std::string& fname, HANDLE file) : fname_(fname), file_(file) {}
	virtual ~WindowsRandomAccessFile() { CloseHandle(file_); }

	virtual Status Read(uint64_t offset, size_t n, Slice* result, char* scratch) const {
		OVERLAPPED overlapped = {0};
		overlapped.Offset = static_cast<DWORD>(offset);
		overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);
		DWORD bytesRead = 0;
		BOOL success = ReadFile(file_, scratch, (DWORD)n, &bytesRead, &overlapped);
		*result = Slice(scratch, bytesRead);
		return success ? Status::OK() : Status::IOError(fname_);
	}
};

class WindowsWritableFile : public WritableFile {
	static const size_t kBufferSize = 65536;
	std::string fname_;
	HANDLE file_;
	size_t pos_;
	BYTE buffer_[kBufferSize];

public:
	WindowsWritableFile(const std::string& fname, HANDLE file) : fname_(fname), file_(file), pos_(0) {}
	virtual ~WindowsWritableFile() { CloseHandle(file_); }

	virtual Status Append(const Slice& data) {
		size_t totalBytesWritten = 0;
		while(totalBytesWritten < data.size()) {
			if(pos_ == kBufferSize) Flush();
			size_t size = std::min(kBufferSize - pos_, data.size() - totalBytesWritten);
			memcpy(buffer_ + pos_, data.data() + totalBytesWritten, size);
			pos_ += size;
			totalBytesWritten += size;
		}
		return Status::OK();
	}

	virtual Status Close() {
		Status status = Flush();
		CloseHandle(file_);
		file_ = INVALID_HANDLE_VALUE;
		return status;
	}

	virtual Status Flush() {
		size_t pos = 0;
		while(pos < pos_) {
			DWORD bytesWritten = 0;
			if(!WriteFile(file_, &buffer_[pos], (DWORD)(pos_ - pos), &bytesWritten, 0))
				return Status::IOError(fname_);
			pos += bytesWritten;
		}
		pos_ = 0;
		return Status::OK();
	}

	virtual Status Sync() {
		Status status = Flush();
		if(!status.ok()) return status;
		return FlushFileBuffers(file_) ? Status::OK() : Status::IOError(fname_);
	}
};

class WindowsFileLock : public FileLock {
	std::string fname_;
	HANDLE file_;

public:
	WindowsFileLock(const std::string& fname, HANDLE file) : fname_(fname), file_(file) {}
	virtual ~WindowsFileLock() { Close(); }
	const std::string& GetFileName() { return fname_; }

	bool Close() {
		bool success = (file_ == INVALID_HANDLE_VALUE || CloseHandle(file_));
		file_ = INVALID_HANDLE_VALUE;
		return success;
	}
};

class WindowsLogger : public Logger {
	WritableFile* log_;

public:
	WindowsLogger(WritableFile* log) : log_(log) {}
	virtual ~WindowsLogger() { delete log_; }

	// Write an entry to the log file with the specified format.
	virtual void Logv(const char* format, va_list ap) {
		const size_t kBufSize = 4096;
		char buffer[kBufSize];
		int written = vsnprintf(buffer, kBufSize - 1, format, ap);
		if(written < 0) written = kBufSize - 1;
		buffer[written++] = '\n';
		log_->Append(Slice(buffer, written));
		log_->Flush();
	}
};

}

class WindowsEnv : public Env {
	LARGE_INTEGER freq_;

public:
	WindowsEnv() { QueryPerformanceFrequency(&freq_); }

	// Create a brand new sequentially-readable file with the specified name.
	// On success, stores a pointer to the new file in *result and returns OK.
	// On failure stores NULL in *result and returns non-OK.  If the file does
	// not exist, returns a non-OK status.  Implementations should return a
	// NotFound status when the file does not exist.
	//
	// The returned file will only be accessed by one thread at a time.
	virtual Status NewSequentialFile(const std::string& fname, SequentialFile** result) {
		*result = 0;
		WCHAR wbuf[MAX_PATH];
		HANDLE file = CreateFileW(Utf8_Wchar(fname, wbuf), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
			0, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
		if(file == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND ? Status::NotFound(fname) : Status::IOError(fname);
		*result = new WindowsSequentialFile(fname, file);
		return Status::OK();
	}

	// Create a brand new random access read-only file with the
	// specified name.  On success, stores a pointer to the new file in
	// *result and returns OK.  On failure stores NULL in *result and
	// returns non-OK.  If the file does not exist, returns a non-OK
	// status.  Implementations should return a NotFound status when the file does
	// not exist.
	//
	// The returned file may be concurrently accessed by multiple threads.
	virtual Status NewRandomAccessFile(const std::string& fname, RandomAccessFile** result) {
		*result = 0;
		WCHAR wbuf[MAX_PATH];
		HANDLE file = CreateFileW(Utf8_Wchar(fname, wbuf), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
			0, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, 0);
		if(file == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND ? Status::NotFound(fname) : Status::IOError(fname);
		*result = new WindowsRandomAccessFile(fname, file);
		return Status::OK();
	}

	// Create an object that writes to a new file with the specified
	// name.  Deletes any existing file with the same name and creates a
	// new file.  On success, stores a pointer to the new file in
	// *result and returns OK.  On failure stores NULL in *result and
	// returns non-OK.
	//
	// The returned file will only be accessed by one thread at a time.
	virtual Status NewWritableFile(const std::string& fname, WritableFile** result) {
		*result = 0;
		WCHAR wbuf[MAX_PATH];
		HANDLE file = CreateFileW(Utf8_Wchar(fname, wbuf), GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
		if(file == INVALID_HANDLE_VALUE) return Status::IOError(fname);
		*result = new WindowsWritableFile(fname, file);
		return Status::OK();
	}

	// Returns true iff the named file exists.
	virtual bool FileExists(const std::string& fname) {
		WCHAR wbuf[MAX_PATH];
		DWORD attr = GetFileAttributesW(Utf8_Wchar(fname, wbuf));
		return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
	}

	// Store in *result the names of the children of the specified directory.
	// The names are relative to "dir".
	// Original contents of *results are dropped.
	virtual Status GetChildren(const std::string& dir, std::vector<std::string>* result) {
		result->clear();
		std::string dirWildcard(dir);
		dirWildcard.append("\\*");
		WIN32_FIND_DATAW fd = {0};
		WCHAR wbuf[MAX_PATH];
		HANDLE h = FindFirstFileW(Utf8_Wchar(dirWildcard, wbuf), &fd);
		if(h == INVALID_HANDLE_VALUE) return Status::IOError(dir);
		char buf[MAX_PATH];
		do {
			if(!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				result->push_back(Wchar_Utf8(fd.cFileName, buf));
		}
		while(FindNextFileW(h, &fd));
		FindClose(h);
		return Status::OK();
	}

	// Delete the named file.
	virtual Status DeleteFile(const std::string& fname) {
		WCHAR wbuf[MAX_PATH];
		BOOL r = DeleteFileW(Utf8_Wchar(fname, wbuf));
		return r ? Status::OK() : Status::IOError(fname);
	}

	// Create the specified directory.
	virtual Status CreateDir(const std::string& dirname) {
		WCHAR wbuf[MAX_PATH];
		return CreateDirectoryW(Utf8_Wchar(dirname, wbuf), 0) ? Status::OK() : Status::IOError(dirname);
	}

	// Delete the specified directory.
	virtual Status DeleteDir(const std::string& dirname) {
		WCHAR wbuf[MAX_PATH];
		SHFILEOPSTRUCTW fileop = {0};
		fileop.wFunc = FO_DELETE;
		fileop.pFrom = Utf8_Wchar(dirname, wbuf);
		fileop.fFlags = 0x14; // FOF_SILENT | FOF_NOCONFIRMATION
		int nResult = SHFileOperationW(&fileop);
		return !nResult && !fileop.fAnyOperationsAborted ? Status::OK() : Status::IOError(dirname);
	}

	// Store the size of fname in *file_size.
	virtual Status GetFileSize(const std::string& fname, uint64_t* file_size) {
		*file_size = 0;
		WIN32_FILE_ATTRIBUTE_DATA fad;
		WCHAR wbuf[MAX_PATH];
		if(!GetFileAttributesExW(Utf8_Wchar(fname, wbuf), GetFileExInfoStandard, &fad))
			return Status::IOError(fname);
		*file_size = (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) + fad.nFileSizeLow;
		return Status::OK();
	}

	// Rename file src to target.
	virtual Status RenameFile(const std::string& src, const std::string& target) {
		WCHAR wbuf1[MAX_PATH], wbuf2[MAX_PATH];
		BOOL r = MoveFileExW(Utf8_Wchar(src, wbuf1), Utf8_Wchar(target, wbuf2), MOVEFILE_REPLACE_EXISTING);
		return r ? Status::OK() : Status::IOError(src, target);
	}

	// Lock the specified file.  Used to prevent concurrent access to
	// the same db by multiple processes.  On failure, stores NULL in
	// *lock and returns non-OK.
	//
	// On success, stores a pointer to the object that represents the
	// acquired lock in *lock and returns OK.  The caller should call
	// UnlockFile(*lock) to release the lock.  If the process exits,
	// the lock will be automatically released.
	//
	// If somebody else already holds the lock, finishes immediately
	// with a failure.  I.e., this call does not wait for existing locks
	// to go away.
	//
	// May create the named file if it does not already exist.
	virtual Status LockFile(const std::string& fname, FileLock** lock) {
		*lock = 0;
		WCHAR wbuf[MAX_PATH];
		HANDLE file = CreateFileW(Utf8_Wchar(fname, wbuf), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_ALWAYS, 0, 0);
		if(file == INVALID_HANDLE_VALUE) return Status::IOError(fname);
		*lock = new WindowsFileLock(fname, file);
		return Status::OK();
	}

	// Release the lock acquired by a previous successful call to LockFile.
	// REQUIRES: lock was returned by a successful LockFile() call
	// REQUIRES: lock has not already been unlocked.
	virtual Status UnlockFile(FileLock* lock) {
		WindowsFileLock* my_lock = dynamic_cast<WindowsFileLock*>(lock);
		Status result;
		if(my_lock && !my_lock->Close())
			result = Status::IOError(my_lock->GetFileName(), "Could not close lock file.");
		delete my_lock;
		return result;
	}

	// Arrange to run "(*function)(arg)" once in a background thread.
	//
	// "function" may run in an unspecified thread.  Multiple functions
	// added to the same Env may run concurrently in different threads.
	// I.e., the caller may not assume that background work items are
	// serialized.
	virtual void Schedule(void (*function)(void* arg), void* arg) {
		QueueUserWorkItem(ThreadProc, new ThreadParam(function, arg), WT_EXECUTEDEFAULT);
	}

	// Start a new thread, invoking "function(arg)" within the new thread.
	// When "function(arg)" returns, the thread will be destroyed.
	virtual void StartThread(void (*function)(void* arg), void* arg) {
		CreateThread(0, 0, ThreadProc, new ThreadParam(function, arg), 0, 0);
	}

	// *path is set to a temporary directory that can be used for testing. It may
	// or many not have just been created. The directory may or may not differ
	// between runs of the same process, but subsequent calls will return the
	// same directory.
	virtual Status GetTestDirectory(std::string* path) {
		WCHAR tempPath[MAX_PATH];
		GetTempPathW(MAX_PATH, tempPath);
		char buf[MAX_PATH];
		*path = Wchar_Utf8(tempPath, buf);
		return Status::OK();
	}

	static uint64_t gettid() {
		return GetCurrentThreadId();
	}

	// Create and return a log file for storing informational messages.
	virtual Status NewLogger(const std::string& fname, Logger** result) {
		WCHAR wbuf[MAX_PATH];
		FILE* fp = _wfopen(Utf8_Wchar(fname, wbuf), L"wb");
		if(!fp) {
			*result = 0;
			return Status::IOError(fname, strerror(errno));
		} else {
			*result = new PosixLogger(fp, gettid);
			return Status::OK();
		}
/*		*result = 0;
		WritableFile* logfile;
		Status status = NewWritableFile(fname, &logfile);
		if(status.ok()) *result = new WindowsLogger(logfile);
		return status;*/
	}

	// Returns the number of micro-seconds since some fixed point in time. Only
	// useful for computing deltas of time.
	virtual uint64_t NowMicros() {
		LARGE_INTEGER count;
		QueryPerformanceCounter(&count);
		return count.QuadPart * 1000000LL / freq_.QuadPart;
	}

	// Sleep/delay the thread for the prescribed number of micro-seconds.
	virtual void SleepForMicroseconds(int micros) {
		// round up to the next millisecond
		Sleep((micros + 999) / 1000);
	}

	// Create an object that either appends to an existing file, or
	// writes to a new file (if the file does not exist to begin with).
	// On success, stores a pointer to the new file in *result and
	// returns OK.  On failure stores NULL in *result and returns
	// non-OK.
	//
	// The returned file will only be accessed by one thread at a time.
	//
	// May return an IsNotSupportedError error if this Env does
	// not allow appending to an existing file.  Users of Env (including
	// the leveldb implementation) must be prepared to deal with
	// an Env that does not support appending.
	virtual Status NewAppendableFile(const std::string& fname, WritableFile** result) {
		*result = 0;
		WCHAR wbuf[MAX_PATH];
		HANDLE file = CreateFileW(Utf8_Wchar(fname, wbuf), GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_ALWAYS, 0, 0);
		if(file == INVALID_HANDLE_VALUE) return Status::IOError(fname);
		if(SetFilePointer(file, 0, 0, FILE_END) == INVALID_SET_FILE_POINTER) {
			CloseHandle(file);
			return Status::IOError(fname);
		}
		*result = new WindowsWritableFile(fname, file);
		return Status::OK();
	}
};

// Return a default environment suitable for the current operating
// system.  Sophisticated users may wish to provide their own Env
// implementation instead of relying on this default environment.
//
// The result of Default() belongs to leveldb and must never be deleted.
Env* Env::Default() {
	static WindowsEnv g_env;
	return &g_env;
}

}

#undef localtime_r

#endif

#ifdef WIN32

#define _WIN32_WINNT 0x500
#include <windows.h>
#undef DeleteFile
#undef min
#include "leveldb/env.h"
#include "port/port.h"

namespace leveldb {
namespace {

struct ThreadParam {
	void (*function)(void* arg);
	void* arg;
	ThreadParam(void (*function)(void* arg), void* arg) : function(function), arg(arg) {}
};

DWORD WINAPI ThreadProc(LPVOID lpParameter) {
	ThreadParam* param = static_cast<ThreadParam*>(lpParameter);
	void (*function)(void* arg) = param->function;
	void* arg = param->arg;
	delete param;
	function(arg);
	return 0;
}

class WindowsSequentialFile : public SequentialFile {
	std::string fname_;
	HANDLE file_;

public:
	WindowsSequentialFile(const std::string& fname, HANDLE file) : fname_(fname), file_(file) {}
	virtual ~WindowsSequentialFile() { CloseHandle(file_); }

	virtual Status Read(size_t n, Slice* result, char* scratch) {
		DWORD bytesRead = 0;
		BOOL success = ReadFile(file_, scratch, n, &bytesRead, 0);
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
		BOOL success = ReadFile(file_, scratch, n, &bytesRead, &overlapped);
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
			if(!WriteFile(file_, &buffer_[pos], pos_ - pos, &bytesWritten, 0))
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
		int written = _vsnprintf(buffer, kBufSize, format, ap);
		log_->Append(Slice(buffer, written >= 0 ? written : kBufSize));
		log_->Append(Slice("\n", 1));
	}
};

}

class WindowsEnv : public Env {
	LARGE_INTEGER freq_;

public:
	WindowsEnv() { QueryPerformanceFrequency(&freq_); }

	// Create a brand new sequentially-readable file with the specified name.
	// On success, stores a pointer to the new file in *result and returns OK.
	// On failure stores 0 in *result and returns non-OK.  If the file does
	// not exist, returns a non-OK status.
	//
	// The returned file will only be accessed by one thread at a time.
	virtual Status NewSequentialFile(const std::string& fname, SequentialFile** result) {
		*result = 0;
		HANDLE file = CreateFileA(fname.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
			0, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
		if(file == INVALID_HANDLE_VALUE) return Status::IOError(fname);
		*result = new WindowsSequentialFile(fname, file);
		return Status::OK();
	}

	// Create a brand new random access read-only file with the
	// specified name.  On success, stores a pointer to the new file in
	// *result and returns OK.  On failure stores 0 in *result and
	// returns non-OK.  If the file does not exist, returns a non-OK
	// status.
	//
	// The returned file may be concurrently accessed by multiple threads.
	virtual Status NewRandomAccessFile(const std::string& fname, RandomAccessFile** result) {
		*result = 0;
		HANDLE file = CreateFileA(fname.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
			0, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, 0);
		if(file == INVALID_HANDLE_VALUE) return Status::IOError(fname);
		*result = new WindowsRandomAccessFile(fname, file);
		return Status::OK();
	}

	// Create an object that writes to a new file with the specified
	// name.  Deletes any existing file with the same name and creates a
	// new file.  On success, stores a pointer to the new file in
	// *result and returns OK.  On failure stores 0 in *result and
	// returns non-OK.
	//
	// The returned file will only be accessed by one thread at a time.
	virtual Status NewWritableFile(const std::string& fname, WritableFile** result) {
		*result = 0;
		HANDLE file = CreateFileA(fname.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
		if(file == INVALID_HANDLE_VALUE) return Status::IOError(fname);
		*result = new WindowsWritableFile(fname, file);
		return Status::OK();
	}

	// Returns true iff the named file exists.
	virtual bool FileExists(const std::string& fname) {
		DWORD attr = GetFileAttributesA(fname.c_str());
		return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
	}

	// Store in *result the names of the children of the specified directory.
	// The names are relative to "dir".
	// Original contents of *results are dropped.
	virtual Status GetChildren(const std::string& dir, std::vector<std::string>* result) {
		result->clear();
		std::string dirWildcard(dir);
		dirWildcard.append("\\*");
		WIN32_FIND_DATAA fd = {0};
		HANDLE h = FindFirstFileA(dirWildcard.c_str(), &fd);
		if(h == INVALID_HANDLE_VALUE) return Status::IOError(dir);
		do {
			if(!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				result->push_back(fd.cFileName);
		}
		while(FindNextFileA(h, &fd));
		FindClose(h);
		return Status::OK();
	}

	// Delete the named file.
	virtual Status DeleteFile(const std::string& fname) {
		return DeleteFileA(fname.c_str()) ? Status::OK() : Status::IOError(fname);
	}

	// Create the specified directory.
	virtual Status CreateDir(const std::string& dirname) {
		return CreateDirectoryA(dirname.c_str(), 0) ? Status::OK() : Status::IOError(dirname);
	}

	// Delete the specified directory.
	virtual Status DeleteDir(const std::string& dirname) {
		std::string dirname2(dirname);
		dirname2.push_back('\0');
		SHFILEOPSTRUCTA fileop = {0};
		fileop.wFunc = FO_DELETE;
		fileop.pFrom = dirname2.c_str();
		fileop.fFlags = 0; // FOF_NO_UI;
		int nResult = SHFileOperationA(&fileop);
		return !nResult && !fileop.fAnyOperationsAborted ? Status::OK() : Status::IOError(dirname);
	}

	// Store the size of fname in *file_size.
	virtual Status GetFileSize(const std::string& fname, uint64_t* file_size) {
		*file_size = 0;
		WIN32_FIND_DATAA fd;
		HANDLE h = FindFirstFileA(fname.c_str(), &fd);
		if(h == INVALID_HANDLE_VALUE) return Status::IOError(fname);
		*file_size = (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) + fd.nFileSizeLow;
		FindClose(h);
		return Status::OK();
	}

	// Rename file src to target.
	virtual Status RenameFile(const std::string& src, const std::string& target) {
		return MoveFileExA(src.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING) ?
			Status::OK() : Status::IOError(src, target);
	}

	// Lock the specified file.  Used to prevent concurrent access to
	// the same db by multiple processes.  On failure, stores 0 in
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
		HANDLE file = CreateFileA(fname.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_ALWAYS, 0, 0);
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
		char tempPath[MAX_PATH];
		GetTempPathA(MAX_PATH, &tempPath[0]);
		*path = tempPath;
		return Status::OK();
	}

	// Create and return a log file for storing informational messages.
	virtual Status NewLogger(const std::string& fname, Logger** result) {
		*result = 0;
		WritableFile* logfile;
		Status status = NewWritableFile(fname, &logfile);
		if(status.ok()) *result = new WindowsLogger(logfile);
		return status;
	}

	// Returns the number of micro-seconds since some fixed point in time. Only
	// useful for computing deltas of time.
	virtual uint64_t NowMicros() {
		LARGE_INTEGER count;
		QueryPerformanceCounter(&count);
		return count.QuadPart * 1000000LL / freq_.QuadPart;
	}

	// Sleep/delay the thread for the perscribed number of micro-seconds.
	virtual void SleepForMicroseconds(int micros) {
		// round up to the next millisecond
		Sleep((micros + 999) / 1000);
	}
};

Env* Env::Default() {
	static WindowsEnv g_env;
	return &g_env;
}

}

#endif

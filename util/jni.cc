#ifdef ENABLE_JNI

#include <stdio.h>
#include <jni.h>
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/options.h"
#include "leveldb/cache.h"
#include "leveldb/write_batch.h"
#include "leveldb/filter_policy.h"
#include "port/port.h"
#include "db/filename.h"

using namespace leveldb;

static ReadOptions			g_ro;		// safe for global shared instance
static WriteOptions			g_wo;		// safe for global shared instance
static const FilterPolicy*	g_fp = 0;	// safe for global shared instance
static WriteBatch			g_wb;		// optimized for single thread writing, fix it if concurrent writing

// public native static long leveldb_open(String path, int write_bufsize, int cache_size);
extern "C" JNIEXPORT jlong JNICALL Java_jane_core_StorageLevelDB_leveldb_1open
	(JNIEnv* jenv, jclass jcls, jstring path, jint write_bufsize, jint cache_size)
{
	if(!path) return 0;
	Options opt;
	opt.create_if_missing = true;
	opt.write_buffer_size = (write_bufsize > 0x100000 ? write_bufsize : 0x100000);
	opt.block_cache = NewLRUCache(cache_size > 0x100000 ? cache_size : 0x100000);
	opt.compression = kNoCompression;
	opt.filter_policy = (g_fp ? g_fp : (g_fp = NewBloomFilterPolicy(10)));
	g_ro.fill_cache = false;
	g_wo.sync = true;
	const char* pathptr = jenv->GetStringUTFChars(path, 0);
	DB* db = 0;
	Status s = DB::Open(opt, pathptr, &db);
	jenv->ReleaseStringUTFChars(path, pathptr);
	return s.ok() ? (jlong)db : 0;
}

// public native static void leveldb_close(long handle);
extern "C" JNIEXPORT void JNICALL Java_jane_core_StorageLevelDB_leveldb_1close
	(JNIEnv* jenv, jclass jcls, jlong handle)
{
	delete (DB*)handle;
}

// public native static byte[] leveldb_get(long handle, byte[] key, int keylen);
extern "C" JNIEXPORT jbyteArray JNICALL Java_jane_core_StorageLevelDB_leveldb_1get
	(JNIEnv* jenv, jclass jcls, jlong handle, jbyteArray key, jint keylen)
{
	DB* db = (DB*)handle;
	if(!db || !key) return 0;
	jsize m = jenv->GetArrayLength(key); if(keylen < m) keylen = m;
	if(keylen <= 0) return 0;
	jbyte* keyptr = jenv->GetByteArrayElements(key, 0);
	if(!keyptr) return 0;
	std::string valstr;
	Status s = db->Get(g_ro, Slice((const char*)keyptr, (size_t)keylen), &valstr);
	jenv->ReleaseByteArrayElements(key, keyptr, JNI_ABORT);
	if(!s.ok()) return 0;
	jsize vallen = valstr.size();
	jbyteArray val = jenv->NewByteArray(vallen);
	jenv->SetByteArrayRegion(val, 0, vallen, (const jbyte*)valstr.data());
	return val;
}

// public native static int leveldb_write(long handle, Iterator<Entry<Octets, OctetsStream>> buf);
extern "C" JNIEXPORT jint JNICALL Java_jane_core_StorageLevelDB_leveldb_1write
	(JNIEnv* jenv, jclass jcls, jlong handle, jobject it)
{
	DB* db = (DB*)handle;
	if(!db || !it) return 1;
	jclass cls_it = jenv->FindClass("java/util/Iterator");
	if(!cls_it) return 2;
	static jclass cls_entry = 0;
	static jclass cls_octets = 0;
	static jmethodID mid_hasNext = 0;
	static jmethodID mid_next = 0;
	static jmethodID mid_getKey = 0;
	static jmethodID mid_getValue = 0;
	static jfieldID fid_buffer = 0;
	static jfieldID fid_count = 0;
	static jint s_err = -1;
	if(s_err < 0)
	{
		s_err = 0;
		jclass cls_entry = jenv->FindClass("java/util/Map$Entry");
		jclass cls_octets = jenv->FindClass("jane/core/Octets");
		if(!cls_entry || !cls_octets) return s_err = 3;
		mid_hasNext = jenv->GetMethodID(cls_it, "hasNext", "()Z");
		mid_next = jenv->GetMethodID(cls_it, "next", "()Ljava/lang/Object;");
		mid_getKey = jenv->GetMethodID(cls_entry, "getKey", "()Ljava/lang/Object;");
		mid_getValue = jenv->GetMethodID(cls_entry, "getValue", "()Ljava/lang/Object;");
		fid_buffer = jenv->GetFieldID(cls_octets, "buffer", "[B");
		fid_count = jenv->GetFieldID(cls_octets, "count", "I");
		if(!mid_hasNext || !mid_next || !mid_getKey || !mid_getValue || !fid_buffer || !fid_count) return s_err = 4;
	}
	else if(s_err > 0) return s_err;
	if(jenv->IsInstanceOf(it, cls_it) == JNI_FALSE) return 5;
	while(jenv->CallBooleanMethod(it, mid_hasNext) == JNI_TRUE)
	{
		jobject entry = jenv->CallObjectMethod(it, mid_next);
		jobject key = jenv->CallObjectMethod(entry, mid_getKey);
		if(key)
		{
			jbyteArray keybuf = (jbyteArray)jenv->GetObjectField(key, fid_buffer); // check cast and null?
			jint keylen = jenv->GetIntField(key, fid_count);
			if(keylen > 0)
			{
				jbyte* keyptr = jenv->GetByteArrayElements(keybuf, 0);
				jobject val = jenv->CallObjectMethod(entry, mid_getValue);
				if(val)
				{
					jbyteArray valbuf = (jbyteArray)jenv->GetObjectField(val, fid_buffer); // check cast and null?
					jint vallen = jenv->GetIntField(val, fid_count);
					if(vallen > 0)
					{
						jbyte* valptr = jenv->GetByteArrayElements(valbuf, 0);
						g_wb.Put(Slice((const char*)keyptr, (size_t)keylen), Slice((const char*)valptr, (size_t)vallen));
						jenv->ReleaseByteArrayElements(valbuf, valptr, JNI_ABORT);
					}
					else
						g_wb.Delete(Slice((const char*)keyptr, (size_t)keylen));
				}
				else
					g_wb.Delete(Slice((const char*)keyptr, (size_t)keylen));
				jenv->ReleaseByteArrayElements(keybuf, keyptr, JNI_ABORT);
			}
		}
	}
	Status s = db->Write(g_wo, &g_wb);
	g_wb.Clear();
	return s.ok() ? 0 : 6;
}

static int64_t AppendFile(Env& env, const std::string& srcfile, const std::string& dstfile)
{
	uint64_t srcsize = 0, dstsize = 0;
	env.GetFileSize(srcfile, &srcsize);
	env.GetFileSize(dstfile, &dstsize);
	if(srcsize <= dstsize) return 0;
	SequentialFile* sf = 0;
	if(!env.NewSequentialFile(srcfile, &sf).ok() || !sf) return -11;
	if(!sf->Skip(dstsize).ok()) { delete sf; return -12; }
	FILE* fp = fopen(dstfile.c_str(), "wb+");
	if(!fp) { delete sf; return -13; }
	fseek(fp, dstsize, SEEK_SET);
	const size_t BUFSIZE = 65536;
	Slice slice; Status s; size_t size; int64_t res = 0; char buf[BUFSIZE];
	do
	{
		s = sf->Read(BUFSIZE, &slice, buf);
		size = slice.size();
		if(size <= 0) break;
		if(fwrite(buf, 1, size, fp) != size) { fclose(fp); delete sf; return -14; }
		res += size;
	}
	while(s.ok());
	fclose(fp);
	delete sf;
	return res;
}

// public native static long leveldb_backup(String srcpath, String dstpath, String datetime);
extern "C" JNIEXPORT jlong JNICALL Java_jane_core_StorageLevelDB_leveldb_1backup
	(JNIEnv* jenv, jclass jcls, jstring srcpath, jstring dstpath, jstring datetime)
{
	if(!srcpath || !dstpath || !datetime) return -1;
	Env* env = Env::Default();
	if(!env) return -2;
	const char* srcpathptr = jenv->GetStringUTFChars(srcpath, 0);
	if(!srcpathptr) return -3;
	std::string srcpathstr(srcpathptr);
	jenv->ReleaseStringUTFChars(srcpath, srcpathptr);
	const char* dstpathptr = jenv->GetStringUTFChars(dstpath, 0);
	if(!dstpathptr) return -4;
	std::string dstpathstr(dstpathptr);
	jenv->ReleaseStringUTFChars(dstpath, dstpathptr);
	const char* datetimeptr = jenv->GetStringUTFChars(datetime, 0);
	if(!datetimeptr) return -5;
	std::string datetimestr(datetimeptr);
	jenv->ReleaseStringUTFChars(datetime, datetimeptr);

	jlong n = 0;
	std::vector<std::string> files;
	std::string files_saved;
	env->CreateDir(dstpathstr);
	g_mutex_backup.Lock();
	if(!env->GetChildren(srcpathstr, &files).ok()) { g_mutex_backup.Unlock(); return -6; }
	for(std::vector<std::string>::const_iterator it = files.begin(), ie = files.end(); it != ie; ++it)
	{
		uint64_t num;
		FileType ft;
		if(!ParseFileName(*it, &num, &ft)) continue;
		if(ft == kTableFile || ft == kLogFile || ft == kDescriptorFile)
		{
			int64_t r = AppendFile(*env, srcpathstr + '/' + *it, dstpathstr + '/' + *it);
			if(r > 0) n += r; // log error?
			files_saved.append(*it);
			files_saved.append("\n");
		}
		else if(ft == kCurrentFile)
		{
			int64_t r = AppendFile(*env, srcpathstr + '/' + *it, dstpathstr + '/' + *it + '-' + datetimestr);
			if(r > 0) n += r; // log error?
			files_saved.append(*it + '-' + datetimestr);
			files_saved.append("\n");
		}
		// [optional] copy LOG file to backup dir and rename to LOG-[datetime]
	}
	g_mutex_backup.Unlock();
	WritableFile* wf = 0;
	if(!env->NewWritableFile(dstpathstr + '/' + "BACKUP" + '-' + datetimestr, &wf).ok()) return -7;
	if(!wf->Append(Slice(files_saved)).ok()) return -8;
	if(!wf->Sync().ok()) return -9;
	wf->Close();
	delete wf;
	return n;
}

/* TODO:
* iterator interface:
1. new/delete iterator with new/delete snapshot
2. set iterator to first/last/key/next/prev and return key/value
*/

#endif

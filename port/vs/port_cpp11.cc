#include "port/port_posix.h"

#include <cstdlib>
#include <stdio.h>
#include <string.h>

namespace leveldb {
namespace port {

Mutex::Mutex() {}
Mutex::~Mutex() {}

void Mutex::Lock() { mu_.lock(); }
void Mutex::Unlock() { mu_.unlock(); }

CondVar::CondVar(Mutex* mu) : mu_(mu) {}
CondVar::~CondVar() {}

void CondVar::Wait() { cv_.wait(mu_->mu_); }
void CondVar::Signal() { cv_.notify_one(); }
void CondVar::SignalAll() { cv_.notify_all(); }

void InitOnce(OnceType* once, void(*initializer)()) { std::call_once(**once, initializer); }

}  // namespace port
}  // namespace leveldb

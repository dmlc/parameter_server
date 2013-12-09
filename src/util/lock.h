#pragma once

#include <pthread.h>
#include <stdio.h>

namespace PS {

class Mutex {
 public:
  Mutex() {
    pthread_mutex_init(&mutex_, NULL);
  }
  ~Mutex() {
    pthread_mutex_destroy(&mutex_);
  }
  void Lock() {
    pthread_mutex_lock(&mutex_);
  }
  void Unlock() {
    pthread_mutex_unlock(&mutex_);
  }
  pthread_mutex_t *GetRawMutex() {
    return &mutex_;
  }
 private:
  pthread_mutex_t mutex_;
};

class RWLock {
 public:
  RWLock() {
    pthread_rwlock_init(&rwlock_, NULL);
  }
  ~RWLock() {
    pthread_rwlock_destroy(&rwlock_);
  }
  void RLock() {
    pthread_rwlock_rdlock(&rwlock_);
  }
  void WLock() {
    pthread_rwlock_wrlock(&rwlock_);
  }
  void Unlock() {
    pthread_rwlock_unlock(&rwlock_);
  }
 private:
  pthread_rwlock_t rwlock_;
};

class ScopeLock {
 public:
  ScopeLock(Mutex *mutex) {
    mutex_ = mutex;
    mutex_->Lock();
  }
  ~ScopeLock() {
    mutex_->Unlock();
  }
 private:
  Mutex *mutex_;
};

class CondVar {
 public:
  explicit CondVar(Mutex *mutex) {
    mutex_ = mutex;
    pthread_cond_init(&cond_, NULL);
  }
  ~CondVar() {
    pthread_cond_destroy(&cond_);
  }
  //Wait for condition
  void Wait() {
    pthread_mutex_t *raw_mutex = mutex_->GetRawMutex();
    pthread_cond_wait(&cond_, raw_mutex);
  }
  // Wakeup one wait
  void Signal() {
    pthread_cond_signal(&cond_);
  }
  // Wakeup all waits
  void SignalAll() {
    pthread_cond_broadcast(&cond_);
  }

 private:
  Mutex *mutex_;
  pthread_cond_t cond_;
};

}

#ifndef  __RINGBUFF_H__
#define  __RINGBUFF_H__

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <semaphore.h>
#include <iostream>
#include <errno.h>

namespace discd {

template<class T> class RingBuff {
    public:
        int Init(int ring_size);
        int Write(T *buff);
        int Read(T *buff);
        int TryRead(T *buff);
    private:
        int _nextin;
        int _nextout;
        int _ring_size;
        T *_buff;
        sem_t _occupied;
        sem_t _empty;
        sem_t _pmut;
        sem_t _cmut;
};

template<class T>
int RingBuff<T>::Init(int ring_size) {
    _ring_size = ring_size;
    _buff = new(std::nothrow)T[_ring_size];
    if (_buff == NULL) {
        fprintf(stderr, "alloc buff for RingBuff failed");
        goto RING_BUFF_INIT_FAILED;
    }
    sem_init(&_occupied, 0, 0);
    sem_init(&_empty, 0, _ring_size);
    _nextin = 0;
    _nextout = 0;
    sem_init(&_pmut, 0, 1);
    sem_init(&_cmut, 0, 1);
    return 0;
RING_BUFF_INIT_FAILED:
    delete _buff;
    return -1;
}

template<class T>
int RingBuff<T>::Write(T *buff) {
    sem_wait(&_empty);
    sem_wait(&_pmut);
    _buff[_nextin++] = *buff;
    if (_nextin == _ring_size) {
        _nextin = 0;
    }
    sem_post(&_pmut);
    sem_post(&_occupied);
    return 0;
}

template<class T>
int RingBuff<T>::Read(T *buff) {
   sem_wait(&_occupied);
    sem_wait(&_cmut);
    *buff = _buff[_nextout++];
    if (_nextout == _ring_size) {

        _nextout = 0;
    }
    sem_post(&_cmut);
    sem_post(&_empty);
    return 0;
}

/***** no block read, return 1 mean failed to read *****/
template<class T>
int RingBuff<T>::TryRead(T *buff) {
    int ret = sem_trywait(&_occupied);
    if (ret == -1 && errno == EAGAIN) {
        return 1;
    }
   sem_wait(&_cmut);
   *buff = _buff[_nextout++];
   if (_nextout == _ring_size) {       
       _nextout = 0;
   }
   sem_post(&_cmut);
   sem_post(&_empty);
   return 0;
}

}


#endif  //__RINGBUFF_H__


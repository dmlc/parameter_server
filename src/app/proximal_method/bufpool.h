/**
 * mangement a buffer pool. assume there are 'max_t' times, each time we may ask for a bufer
 * with maximal length 'buf_len'.  allocate a max_t * buf_len length buffer is quite
 * expensive.
 */

#ifndef __BUFF_H__
#define __BUFF_H__

#include "common.h"
#include "singleton.h"
#include "talk.h"

namespace discd {

template<class T>
class BufferPool {
  public:
    int Init(int poolsize, int activepoolsize, size_t buflen);
    /**
     * get a buffer from the pool
     */
    T* Get(int pool) {
        if (pool<0 || pool>_poolsize) {
            FATAL("invalid pool [%d], poolsize [%d]",pool, _poolsize);
            return NULL;
        }

        pthread_mutex_lock(&_lock);
        /**
         * has been assigned already
         */
        if (_activepool[pool]>=0) {
            pthread_mutex_unlock(&_lock);
            if (_activepool[pool] >= _activepoolsize) {
                FATAL("error ...");
                return NULL;
            }
            return _bufs[_activepool[pool]];
        }
        /**
         * get an available one from the pool
         * TODO: use a stack here...
         */
        for (int i = 0; i < _activepoolsize; i++) {
            if (_is_inuse[i])
                continue;
            _activepool[pool] = i;
            _is_inuse[i] = 1;
            pthread_mutex_unlock(&_lock);
            return _bufs[i];
        }
        FATAL("no available buffer... return NULL, you may crash...");
        pthread_mutex_unlock(&_lock);
        return NULL;
    }

    /**
     * just remove the pointer
     */
    int Delete(int pool) {
        ASSERT(pool>=0&&pool<_poolsize, "invalid pool [%d], poolsize [%d]",
               pool, _poolsize);
        pthread_mutex_lock(&_lock);
        int i = _activepool[pool];
        if (i >=0 && i < _activepoolsize)
            _is_inuse[i] = 0;
        _activepool[pool] = -1;
        pthread_mutex_unlock(&_lock);
        return 0;
    }
  private:
    int _poolsize;
    // size_t _buflen;
    int _activepoolsize;
    int *_activepool;
    int *_is_inuse;
    T **_bufs;
    pthread_mutex_t _lock;
};

template <class T>
int BufferPool<T>::Init(int poolsize, int activepoolsize, size_t buflen) {
    _poolsize = poolsize;
    _activepoolsize = activepoolsize;
    ASSERT(pthread_mutex_init(&_lock, NULL)==0);

    NEW_SET1(_activepool, int, _poolsize);
    NEW_SET0(_is_inuse,   int, _activepoolsize);
    NEW(_bufs, T*, _activepoolsize);
    for (int i = 0; i < _activepoolsize; i++) {
        NEW(_bufs[i], T, buflen);
    }
    return 0;
}



}

#endif

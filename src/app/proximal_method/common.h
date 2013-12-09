#ifndef  __COMMON_H__
#define  __COMMON_H__

// #define _DEBUG_

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <assert.h>
#include <semaphore.h>
#include <iostream>

namespace discd {


struct parameter_t {
    int    rank_size;
    int    my_rank;
    int    net_port;
    double lambda;
    double Delta_init;
    int  N;                     // number of sets
    int delay;                  // maximal number of delayed sets
    uint32_t thread_num;
    char *data;
    char *cache_data;
    int max_iter;
    double stop;
    double alpha;
    int H;
    size_t n;
    int shrk_on;
    unsigned int seed;
};

/********************************************************************
 *  some template
 ********************************************************************/

#ifndef min
#define min(x,y) ( ((x) < (y)) ? (x) : (y) )
#endif

#ifndef max
#define max(x,y) ( ((x) > (y)) ? (x) : (y) )
#endif

#define NOTICE(_fmt_,args...)                                   \
    do {                                                        \
        fprintf(stdout, "NOTICE:\t" _fmt_ "\n", ##args);        \
    } while (0)
#define DEBUG(_fmt_,args...)                                    \
    do {                                                        \
        fprintf(stderr, "DEBUG: %12s:%5d : " _fmt_ "\n",       \
                __FILE__,__LINE__,   ##args);      \
    } while (0)

// __FUNCTION__,
#define FATAL(_fmt_,args...)                            \
    do {                                                \
        fprintf(stderr, "FATAL:%13s:%5d:\t" _fmt_ "\n", \
                __FILE__, __LINE__, ##args);            \
    } while (0)

#define ASSERT(_val_, args...)                  \
    do {                                        \
        int v = (_val_);                        \
        if (v ==0 ) {                     \
            FATAL(args);                        \
            return -1;                          \
        }                                       \
    } while (0);

        // DEBUG("new mem: %lu", (len+10)*sizeof(type));
//    FATAL("new %lu", (len+10)*sizeof(type));
#define NEW(var,type,len)                                               \
    do {                                                                \
        size_t size = (len+10)*sizeof(type);                            \
        (var) = (type *) malloc(size);                                  \
        ASSERT(var!=NULL, "failed to malloc [%lu] memory", size);       \
    } while (0)

#define NEW_SET0(var, type, len)                \
    do {                                        \
        NEW((var), type,len);                   \
        memset((var), 0, sizeof(type)*len);     \
    } while (0);

#define NEW_SET1(var, type, len)                \
    do {                                        \
        NEW((var), type,len);                   \
        memset((var), -1, sizeof(type)*len);     \
    } while (0);

#define RESIZE(var, type, len)                                          \
    do {                                                                \
        (var) = (type *)realloc((void *)(var), ((len)+10)*sizeof(type)); \
        ASSERT((var)!=NULL, "out of memory");                           \
    } while (0)

#define DELETE(var) do {free((var)); (var)=NULL; } while (0);

/**
 * bitmap, basic type is uint16_t, the first two entries are used to
 * store the length, which is uint32_t
 */

typedef uint16_t bitmap_t;

static const uint32_t BITMAP_SHIFT = 4;
static const uint32_t BITMAP_MASK = 0x0F;

inline int bitmap_new(uint32_t n, bitmap_t **bitmap) {
    uint32_t len = (n>>BITMAP_SHIFT)+3;
    NEW_SET0(*bitmap,bitmap_t,len);
    uint32_t *v = (uint32_t*) *bitmap;
    v[0] = n;
    return 0;
}

inline void bitmap_set(uint32_t i, bitmap_t *bitmap)
{
    bitmap[(i>>BITMAP_SHIFT)+2] |= (bitmap_t)(1<<(i&BITMAP_MASK));
}

inline void bitmap_clr(uint32_t i, bitmap_t *bitmap)
{
    bitmap[(i>>BITMAP_SHIFT)+2] &= ~((bitmap_t)(1<<(i&BITMAP_MASK)));
}

//inline int bitmap_test(int i, bitmap_t *bitmap) __attribute__((always_inline));
inline int bitmap_test(uint32_t i, bitmap_t *bitmap)
{
    return (bitmap[(i>>BITMAP_SHIFT)+2] >> (i&BITMAP_MASK)) & 1;
}

/**
 * return # bit entries
 */
inline uint32_t bitmap_len(bitmap_t *bitmap) {
    uint32_t *v = (uint32_t*) bitmap;
    return v[0];
}

/**
 * return the memory size
 */
inline uint32_t bitmap_size(bitmap_t *bitmap) {
    uint32_t n = bitmap_len(bitmap);
    return (n>>BITMAP_SHIFT)+3;
}

#define BITCOUNT(x) (((BX_(x)+(BX_(x)>>4)) & 0x0F0F0F0F) % 255)
#define BX_(x) ((x) - (((x)>>1)&0x77777777)     \
                - (((x)>>2)&0x33333333)         \
                - (((x)>>3)&0x11111111))

static unsigned char LUT[65536];
static bool init_bitmap_nnz = false;
inline uint32_t bitmap_nnz(uint32_t start, uint32_t end, bitmap_t *bitmap)
{
    ASSERT(start <= end);
    uint32_t nnz = 0;
    for (uint32_t i = start; i < end; i++) {
        nnz += bitmap_test(i, bitmap);
    }
    return nnz;
}

inline uint32_t bitmap_nnz(bitmap_t *bitmap)
{
    if (!init_bitmap_nnz) {
        for(int i=0;i<65536;i++) {
            LUT[i] = (unsigned char)BITCOUNT(i);
        }
        init_bitmap_nnz = true;
    }

    uint32_t n = bitmap_len(bitmap);
    uint32_t bn = n>>BITMAP_SHIFT;
    uint32_t v = 0;
    for (uint32_t i = 2; i < bn+2; i++) {
        v += LUT[bitmap[i]];
    }

    return v + bitmap_nnz((bn<<BITMAP_SHIFT), n, bitmap);
}


inline int bitmap_reset1(bitmap_t *bitmap)
{
    uint32_t size = bitmap_size(bitmap)-2;
    memset(bitmap+2, 0xFF, size*sizeof(bitmap_t));
    return 0;
}

inline int bitmap_reset1(uint32_t start, uint32_t end, bitmap_t *bitmap)
{
    for (uint32_t i = start; i < end; i ++) {
        bitmap_set(i, bitmap);
    }
    return 0;
}

inline int bitmap_reset0(bitmap_t *bitmap)
{
    uint32_t size = bitmap_size(bitmap)-2;
    memset(bitmap+2, 0xFF, size*sizeof(bitmap_t));
    return 0;
}

const double NEG_SMALL_VALUE = -1e-20;
const double POS_SMALL_VALUE = 1e-20;

inline bool equal_zero(double val) {
    return val < POS_SMALL_VALUE && val > NEG_SMALL_VALUE;
}


/**********************************************
 *  time
 **********************************************/

inline void tic(struct timeval& start) {
    gettimeofday(&start, NULL);
}

inline double toc(const timeval start) {
    struct timeval end;
    gettimeofday(&end, NULL);
    return double(end.tv_sec-start.tv_sec)+double(end.tv_usec-start.tv_usec)/1e6;
}

template <class T>
T* linspace(T start, T end, int k, T* ret) {
    double itv = (double)(end-start) / (double)k;
    for (int i = 0; i <= k; i++) {
        ret[i] = (T) round(itv*(double)i) + start;
    }
    return ret;
}

template <class T>
T* linspace(T start, T end, int k) {
    assert(k >=0);
    T *ret = new T[k+5];
    if (k == 0)
        return ret;
    return linspace(start, end, k, ret);
}

template <class T>
T* linspace(T length, int k) {
    return linspace(0, length, k);
}


template <class T>
int randperm(T *vct, size_t len) {
    //    srand(time(NULL));
    for (size_t j = 0; j < len; j++) {
        size_t i = (size_t)rand()%len;
        T tmp = vct[i];
        vct[i] = vct[j];
        vct[j] = tmp;
    }
    return 0;
}

template <class T>
T* randperm(size_t len) {
    T *vct = new T[len+5];
    if (vct==NULL)
        return NULL;
    for (size_t i = 0; i < len; i++)
        vct[i] = i;
    randperm(vct, len);
    return vct;
}

template <class T>
void prvec(T *vec, size_t n) {
    std::cout << "Array:\t";
    for (size_t i = 0; i < n; i++) {
        std::cout << vec[i];
        if (i < n - 1)
            std::cout << " ";
        else
            std::cout << std::endl;
    }
}

}

#endif

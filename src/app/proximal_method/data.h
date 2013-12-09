#ifndef __DATA_H__
#define __DATA_H__
#include "common.h"

namespace discd {

template <class T>
long load_bin(char *filename, T **buf, long offset, size_t n) {

    FILE *fp = fopen(filename, "rb");
    ASSERT(fp!=NULL, "open %s for reading failed", filename);

    if (*buf == NULL) {
        NEW(*buf, T, n);
    }

    if (offset != 0) {
        if (fseek(fp, offset*sizeof(T), SEEK_SET) != 0) {
            FATAL("fseek failed");
            return -1;
        }
    }

    size_t ret = fread(*buf,sizeof(T),n,fp);
    fclose(fp);

    if (ret != n) {
        FATAL("expect to read [%lu] elements, but only returned [%lu]", n, ret);
        return -1;
    }
    
    return ret;
}

template <class T>
long load_bin(char *filename, T **buf) {

    struct stat file_stat;
    if (stat(filename, &file_stat) < 0) {
        FATAL("stat %s failed", filename);
        return -1;
    }
    size_t n = file_stat.st_size / sizeof(T);

    return load_bin<T>(filename, buf, 0, n);
}

template <class T>
long save_bin(char *filename, T *buff, size_t n) {
    FILE *fp = fopen(filename,"wb");
    if (fp == NULL) {
        FATAL("open %s for writing failed", filename);
        return -1;
    }
    
    size_t ret = fwrite(buff,sizeof(T),n,fp);
    if (ret != n) {
        FATAL("only write [%lu] bytes of [%lu] bytes to [%s]", 
              ret, n, filename);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return ret;
}

inline int load_data(char *filename, void **buff, size_t &len, long offset) {

    FATAL("old version");
    if (len == (size_t)-1) {
        struct stat file_stat;
        if (stat(filename, &file_stat) < 0) {
            FATAL("stat %s failed", filename);
            return -1;
        }
        len = file_stat.st_size;
    }
    
    if (*buff == NULL) {
        *buff = (char *) malloc((len+100)*sizeof(char));
        if (*buff == NULL) {
            FATAL("no mem to read %lu bytes from %s",
                      len,filename);
            return -1;
        }
    }
    FILE *fp = fopen(filename,"rb");
    if (fp == NULL) {
        FATAL("open %s for reading failed", filename);
        return -1;
    }
    if (offset != 0) {
        if (fseek(fp, offset, SEEK_SET) != 0) {
            FATAL("fseek failed");
            return -1;
        }
    }

    ///// a bug for max os x, when len ~ 2^31, buff get all 0s
    // size_t ret = fread(*buff,1,len,fp);

    char   *ptr          = (char *)*buff;
    size_t  to_read      = len;
    size_t  max_read_len = 1 << 30;

    while (to_read > 0) {
        size_t read_some = min(to_read, max_read_len);
        size_t ret = fread(ptr,1,read_some,fp);

        if (ret != read_some) {
            FATAL("only read %lu bytes of %lu bytes from %s",
                  ret, read_some, filename);
            fclose(fp);
            return -1;
        }
        
        to_read -= read_some;
        ptr     += read_some;
    }

    fclose(fp);
    return 0;
}

inline int save_data(char *filename, void *buff, size_t len) {
    FILE *fp = fopen(filename,"wb");
    if (fp == NULL) {
        FATAL("open %s for writing failed", filename);
        return -1;
    }
    
    size_t ret = fwrite(buff,1,len,fp);
    if (ret != len) {
        FATAL("only write %lu bytes of %lu bytes to %s", 
              ret, len, filename);
        return -1;
    }
    fclose(fp);
    return 0;
}


}

#endif


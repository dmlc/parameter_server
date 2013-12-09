#ifndef __TALKP_H__
#define __TALKP_H__

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <net/if.h>
#include <netinet/in.h>
//#include <linux/types.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <exception>
#include <unistd.h>

#include "common.h"
#include "singleton.h"
#include "ringbuff.h"
#include "data.h"

namespace discd {

inline int writen(int fd, const char *buf, size_t n) {
    ssize_t nleft;
    ssize_t nwritten;
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, buf, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR) {
                nwritten = 0;
            } else {
                FATAL("write socket [%d] failed, errno: [%d]\n", fd, errno);
                return -1;
            }
        }
        nleft -= nwritten;
        buf += nwritten;
    }
    return 0;
}

inline int readn(int fd, char *buf, size_t n) {
    ssize_t nleft;
    ssize_t nread;
    
    nleft = n;
    while (nleft > 0) {
        if ((nread=read(fd,buf,nleft)) < 0) {
            if (errno == EINTR) {
                nread = 0;
            } else {
                FATAL("read socket [%d] failed, errno: [%d]\n", fd, errno);
                return -1;
            }
        } else if (nread == 0) {
            break; /*EOF*/
        }
        nleft -= nread;
        buf += nread;
    }
    return 0;
}

struct pkghead_t {
    int data_type;
    int time;
    size_t buf_len;
};

struct pkg_t {    
    int     data_type;
    int     time;
    int     dest;
    int     orig;
    char   *buf;
    size_t  buf_len;
};
enum STATUS_TYPE {
    GRAD,
    UPDT
};

class TalkConn {
    DECLARE_SINGLETON(TalkConn);
public:
    static const int TALK_SEND_RANK_BUF_LEN = 10;
    static const int IP_SIZE = 50;
public:
    int Init(parameter_t *param);
    int SendRank();
    int Connect();
private:
    int SetupConn();
    int GetIP(char *outip);
    int CreateIPTable();        
    int _rank_size;
    int _my_rank;
    int _port;
};

class Talk {
    DECLARE_SINGLETON(Talk);
public:
    friend class TalkConn;

    int TalkSend(int dest);     /*  */
    int TalkRecv(int orig);    

    int Init(parameter_t *param);
    int Exit() {
        return 0;
    }
    
    inline int SendStatus(int dest, int time, int data_type, char *buf, size_t buf_len) {
        pkg_t pkg;
        pkg.orig      = _my_rank;
        pkg.dest      = dest;
        pkg.time      = time;
        pkg.data_type = data_type;
        pkg.buf       = buf;
        pkg.buf_len   = buf_len;
        _send_status_ring[dest].Write(&pkg);
        return 0;
    }
    
    inline int RecvGrad(pkg_t *pkg) {
        return _recv_grad_ring.Read(pkg);
    }
    inline int RecvUpdt(pkg_t *pkg) {
        return _recv_updt_ring.Read(pkg);
    }

    inline int FreePkg(pkg_t *pkg) {
        if (pkg->buf) {
            DELETE(pkg->buf);
        }        
        return 0;
    }


    /**
     * return 1 means no one is returned
     */

    inline int TryRecvUpdt(pkg_t *pkg) {
        return _recv_updt_ring.TryRead(pkg);
    }
        
private:

    int _rank_size;
    int _my_rank;

    int _talk_exit;
        
    int *_send_conn_table;
    int *_recv_conn_table;

    RingBuff<pkg_t> _recv_grad_ring;
    RingBuff<pkg_t> _recv_updt_ring;
    RingBuff<pkg_t> *_send_status_ring;
    
    char (*_iptable)[TalkConn::IP_SIZE];

    pthread_t *_netp;
};


}


#endif

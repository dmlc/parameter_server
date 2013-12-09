#include "talk.h"

namespace discd {

IMPLEMENT_SINGLETON(Talk);
IMPLEMENT_SINGLETON(TalkConn);

/*********************************************************************
 *               Talk
 *********************************************************************/
void *ThreadTalkSend(void *args)
{
    int dest = (int)(int64_t)(args);
    int ret = Talk::instance()->TalkSend(dest);
    if (ret != 0) {
        FATAL("TalkSend failed.");
    }
    return (void *)(int64_t)(ret);
}

void *ThreadTalkRecv(void *args)
{
    int orig = (int)(int64_t)(args);
    int ret = Talk::instance()->TalkRecv(orig);
    if (ret != 0) {
        FATAL("TalkRecv failed. orig %d", orig);
    }
    return (void *)(int64_t)(ret);
}

int Talk::Init(parameter_t *param)
{
    _rank_size   = param->rank_size;
    _my_rank     = param->my_rank;
    _talk_exit   = 0;
    
    
    try {
        _iptable          = new char[_rank_size][TalkConn::IP_SIZE];
        _send_conn_table  = new int[_rank_size];
        _recv_conn_table  = new int[_rank_size];
        _send_status_ring = new RingBuff<pkg_t>[_rank_size];
        _netp             = new pthread_t[_rank_size*2];
    } catch (std::bad_alloc& ba) {
        FATAL("out of memory: %s", ba.what());
        return -1;
    }

    // TODO is buffer size correct?
    int N = param->N + param->delay;
    int ret = 0;
    ret += _recv_grad_ring.Init(_rank_size*N);
    ret += _recv_updt_ring.Init(_rank_size*N);
    for (int i = 0; i < _rank_size; i++) {
        _send_status_ring[i].Init(2*N);
    }
    
    if (ret != 0) {
        FATAL("init ring buff failed");
        return -1;
    }

    TalkConn *tc = TalkConn::instance();
    if (tc->Init(param) < 0) {
        FATAL("Init TalkConn failed");
        return -1;
    }

    // prvec<int>(_send_conn_table, _rank_size);
    // prvec<int>(_recv_conn_table, _rank_size);
    
    for (int i = 0; i < _rank_size; i++) {
        pthread_create(_netp+i*2,   NULL, ThreadTalkSend, (void *)(int64_t)i);
        pthread_create(_netp+i*2+1, NULL, ThreadTalkRecv, (void *)(int64_t)i);
    }

    return 0;
}

int Talk::TalkSend(int dest)
{
    pkghead_t *head;
    pkg_t     pkg;
    int       fd = _send_conn_table[dest];
    RingBuff<pkg_t> *send_ring = _send_status_ring + dest;
    
    while (1) {
        if (_talk_exit) {
            break;
        }
        send_ring->Read(&pkg);
        head = (pkghead_t *) (pkg.buf - sizeof(pkghead_t));
        head->data_type = pkg.data_type;
        head->time = pkg.time;
        head->buf_len = pkg.buf_len;
#ifdef _DEBUG_
        // DEBUG("send pkg to [%d], time [%d], type [%d], len [%ld]", dest, pkg.time,
        // head->data_type, head->buf_len);
#endif

        if (writen(fd, (char *)head, pkg.buf_len+sizeof(pkghead_t)) != 0) {
            FATAL("send package to rank [%d] faied", dest);
            return -1;
        }
    }
    return 0;
}

// receive package from rank orig
int Talk::TalkRecv(int orig)
{
    pkghead_t head;
    pkg_t     pkg;
    int       fd = _recv_conn_table[orig];
    while (1) {
        if (_talk_exit) {
            break;
        }
        if (readn(fd, (char *)&head, sizeof(pkghead_t)) != 0) {
            FATAL("read package head error");
            return -1;
        }
#ifdef _DEBUG_
        // DEBUG("recv pkg from [%d], time [%d], type [%d], len [%ld]", orig, head.time,
        //       head.data_type, head.buf_len);
#endif

        pkg.dest      = _my_rank;
        pkg.orig      = orig;
        pkg.data_type = head.data_type;
        pkg.time      = head.time;
        pkg.buf_len   = head.buf_len;

        /////////   should be deleted after using it, by calling FreePkg    /////////

        NEW(pkg.buf, char, pkg.buf_len);

        if (readn(fd, pkg.buf, pkg.buf_len) != 0) {
            FATAL("read package content from node [%d] error", orig);
            return -1;
        }
        
        if (pkg.data_type == GRAD) {
            _recv_grad_ring.Write(&pkg);
        }
        else if (pkg.data_type == UPDT) {
            _recv_updt_ring.Write(&pkg);
        } else {
            FATAL("error data_type [%d]", pkg.data_type);
            return -1;
        }
    }
    return 0;
}

/*********************************************************************
 *               TalkConn, setup connections
 *********************************************************************/

int TalkConn::Init(parameter_t *param) {
    _rank_size = param->rank_size;
    _my_rank   = param->my_rank;
    _port      = param->net_port;
    
    if (CreateIPTable() < 0) {
        FATAL("CreateIPTable failed");
        return -1;
    }
    MPI_Barrier(MPI_COMM_WORLD);
    if (SetupConn() < 0) {
        FATAL("SetupConn failed");
        return -1;
    }
    MPI_Barrier(MPI_COMM_WORLD);
    return 0;
}

int TalkConn::CreateIPTable() {
    Talk *talk = Talk::instance();
    char local_ip[IP_SIZE];
    if (GetIP(local_ip) != 0) {
        FATAL("get ip failed");
        return -1;
    }
    DEBUG("r [%d], ip %s", _my_rank, local_ip);
    //// cannot use MPI_Allgather does not garantee the order...
    // if (MPI_Allgather(local_ip, IP_SIZE, MPI_BYTE, talk->_iptable, IP_SIZE, MPI_BYTE, MPI_COMM_WORLD) != 0) {
    //     FATAL("Allgather ip failed");
    //     return -1;
    // }

    int *rv = new int[_rank_size];
    int *ds = new int[_rank_size];
    for (int i=0; i < _rank_size; i++) {
        rv[i] = IP_SIZE;
        ds[i] = IP_SIZE*i;
    }

    if (MPI_Allgatherv(local_ip, IP_SIZE, MPI_BYTE, talk->_iptable, rv, ds,  MPI_BYTE, MPI_COMM_WORLD) != 0) {
        FATAL("Allgather ip failed");
        return -1;
    }

    // DEBUG("r[%d] %s %s %s %s", _my_rank, talk->_iptable[0], talk->_iptable[1], talk->_iptable[2], talk->_iptable[3]);
    return 0;
}

int TalkConn::GetIP(char *outip) {
    int i = 0;
    int sockfd;
    char buf[512];
    char* ip;
    struct ifconf ifconf;
    struct ifreq *ifreq;
    ifconf.ifc_len = 512;
    ifconf.ifc_buf = buf;

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0))<0) {
        FATAL("create socket failed in GetIP, errno: %d", errno);
        return -1;
    }
    if (ioctl(sockfd, SIOCGIFCONF, &ifconf) < 0) { 
        FATAL("ioctl SIOCGIFCONF failed. errno: %d", errno);
        return -1;
    }
    close(sockfd);

    ifreq = (struct ifreq*)buf;
    for (i = (ifconf.ifc_len / sizeof(struct ifreq)); i>0; i--) {
        ip = inet_ntoa(((struct sockaddr_in*)&(ifreq->ifr_addr))->sin_addr);
        if(strcmp(ip,"127.0.0.1") == 0) {
            ifreq++;
            continue;
        }
        strncpy(outip,ip,IP_SIZE);
        outip[4] = '3';
        break;
    }
    return 0;
}

void *ThreadSendRank(void *args) {
    int ret = TalkConn::instance()->SendRank();
    return (void *)(int64_t)ret;
}

void *ThreadConnect(void *args) {
    int ret = TalkConn::instance()->Connect();
    return (void *)(int64_t)ret;
}

int TalkConn::SendRank() {
    Talk *talk = Talk::instance();
    // char send_buf[TALK_SEND_RANK_BUF_LEN];
    int ret = 0;
    // snprintf(send_buf, TALK_SEND_RANK_BUF_LEN, "%d", _my_rank);    
    for (int i = 0; i < _rank_size; i++) {
        // if (writen(talk->_send_conn_table[i], send_buf, TALK_SEND_RANK_BUF_LEN) < 0) {
        if (writen(talk->_send_conn_table[i], (char *)&_my_rank, sizeof(int)) < 0) {
            ret = -1;
        }
    }
    return ret;
}

int TalkConn::Connect() {
    // sleep(1);
    Talk *talk = Talk::instance();
    int sockfd;
    int ret = 0;
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    for (int i = 0; i < _rank_size; i++) {
        ////// connect to node i //////
        servaddr.sin_port = htons(_port+i);
        inet_pton(AF_INET, talk->_iptable[i], &servaddr.sin_addr);
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        int r = connect(sockfd, (sockaddr*)(&servaddr), sizeof(servaddr));
        ret += r;
        if (r != 0) {
            FATAL("errno: %d, r[%d] connects to r[%d] %s failed", 
                  errno, _my_rank, i, talk->_iptable[i]);
        } else {
            // DEBUG("r[%d] connected to r[%d], %s", _my_rank, i, talk->_iptable[i]);
        }
        talk->_send_conn_table[i] = sockfd;
    }
    return ret;
}


int TalkConn::SetupConn() {
    Talk *talk = Talk::instance();
    int sockopt_on = 1;
    int opt_ret = 0;
    int sd_ret = 0;
    int sc_ret = 0;
    pthread_t pid;
    int *recv_conn_table_temp =  new(std::nothrow)int[_rank_size];
    if (recv_conn_table_temp == NULL) {
        FATAL("recv_conn_table_temp malloc failed");
        return -1;
    }

    ////// listen my port //////
    int listenfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (listenfd < 0) {
        FATAL("create listen socket failed, errno: %d", errno);
        goto SETUP_CONN_OUT;
    }
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(sockaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(_port+_my_rank);
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &sockopt_on, sizeof(int)) < 0) {
        FATAL("setsockopt failed, errno: %d", errno);
        goto SETUP_CONN_OUT;
    }
    if (bind(listenfd, (sockaddr*)&servaddr, sizeof(sockaddr_in)) != 0) {
        FATAL("bind failed, errno: %d", errno);
        goto SETUP_CONN_OUT;
    }
    if (listen(listenfd, 1024) != 0) {
        FATAL("listen failed, errno: %d", errno);
        goto SETUP_CONN_OUT;
    }


    ////// all-2-all connect w/ all nodes
    pthread_create(&pid, NULL, ThreadConnect, NULL);
    // sleep(10);
    for (int i = 0; i < _rank_size; i++) {
        socklen_t len = sizeof(sockaddr);
        recv_conn_table_temp[i] = accept(listenfd, (sockaddr*)&servaddr, &len);
        if (recv_conn_table_temp[i] < 0) {
            FATAL("accept failed, errno: %d", errno);
            goto SETUP_CONN_OUT;
        }
    }
    pthread_join(pid, (void **)(int64_t)sc_ret);
    if (sc_ret != 0) {
        FATAL("error occure in ThreadSendConn");
        goto SETUP_CONN_OUT;
    }
    close(listenfd);

    //reorder recv_conn_table accoding to rank number
    // char temp_res[TALK_SEND_RANK_BUF_LEN];
    pthread_create(&pid, NULL, ThreadSendRank, NULL);
    for (int i = 0; i < _rank_size; i++) {
        int rank;
        // readn(recv_conn_table_temp[i], temp_res, TALK_SEND_RANK_BUF_LEN);
        readn(recv_conn_table_temp[i], (char *)&rank, sizeof(int));
        // talk->_recv_conn_table[atoi(temp_res)] = recv_conn_table_temp[i];
        talk->_recv_conn_table[rank] = recv_conn_table_temp[i];
    }
    pthread_join(pid, (void **)(int64_t)sd_ret);
    if (sd_ret != 0) {
        FATAL("error occure in ThreadSendConn");
        goto SETUP_CONN_OUT;
    }
    for (int i = 0; i < _rank_size; i++) {
        //opt_ret += setsockopt(talk->_send_conn_table[i], SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
        //opt_ret += setsockopt(talk->_recv_conn_table[i], SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
        ///// mac os x has no TCP_QUICKACK 
        opt_ret += setsockopt(talk->_send_conn_table[i], IPPROTO_TCP, TCP_QUICKACK,&sockopt_on, sizeof(int));
        opt_ret += setsockopt(talk->_recv_conn_table[i], IPPROTO_TCP, TCP_NODELAY,&sockopt_on, sizeof(int));
    }
    if (opt_ret < 0) {
        FATAL("setsockopt failed, errno: %d", errno);
        goto SETUP_CONN_OUT;
    }
    delete recv_conn_table_temp;
    return 0;
SETUP_CONN_OUT:
    delete recv_conn_table_temp;
    return -1;
}

}


#ifndef __LR_H__
#define __LR_H__

#include <set>
#include "common.h"
#include "singleton.h"
#include "talk.h"
#include "bufpool.h"

namespace discd {

class ClientProc;

const static size_t PKGHEADERSIZE = sizeof(pkghead_t) + 1;

struct grad_t {
    double G;                   // first-order gradient
    double U;                   // upper bound of second-order gradient
};

typedef uint64_t os_t;
typedef uint32_t ix_t;

// struct spm_t {
//     os_t   *row_os;
//     ix_t   *col_ix;
//     double *val;
// };

/**
 * do calculate the gradient and update exw, each thread runs an instance
 */
class CompClient {
    friend class ClientProc;
  public:
    static const int MERGE_LOCK_NUM = 16;
    int Init(int pid);

    double GetObjv() {
        double objv = 0;
        for (ix_t i = 0; i < _n; i++)
            objv += log(1+1/_exw[i]);
        return objv;
    }
  private:

    int AllFinished(int *count, pthread_mutex_t *lock, int t);
    int BlockGrad(int t);
    int BlockExw(int t);
    int Iter();
    double *_exw;
    int Transpose();

    int _pid;
    ClientProc *_cp;
    int *_is_time_updated;

    /**
     * training data, stored by column
     */
    ix_t _n, _p;
    int _is_binary_feature;
    double   *_Y;
    double   *_val;
    ix_t *_idx;
    os_t *_cnt;

    BufferPool<grad_t> _send_buf;

};

struct client_t {
    ix_t      p, server_p;
    double   *delta;
    bitmap_t *as;
    ix_t      nas;
    int       id;
};

class ClientProc {
    DECLARE_SINGLETON(ClientProc);
public:
    friend class ServerProc;
    friend class CompClient;
    int Init(parameter_t *param);
    int Iter();
    int Invoke(int tid);

private:
    inline double GetObjv()
    {
        double objv = 0;
        for (int i = 0; i < _thread_num; i++) {
            objv += _cclients[i].GetObjv();
        }
        return objv;
    }

    int ResetActiveset(int reset_time);
    int CalcObjv();
    int UpdtAsDelta(int t);
    int ReadProblem(parameter_t *param);

    /**
     * algorithm options
     */
    int _N, _delay, _thread_num, _shrk_on, _H_type;
    int _rank_size, _my_rank;
    double _stop_eps;

    int _max_t;
    ix_t _max_blk_p;
    int *_blk_iter_order;       // iterate *[t] at time t
    int *_blk_server;           // server *[r] holds blk r

    ix_t *_col_cut;           // cut columnes into sets, each time update a set of features
    ix_t *_row_cut;          // cut rows into block, each each thread do a block

    /**
     * status
     */

    double _objv;
    size_t _nnz_as, _nnz_w;

    client_t *_blks;
    pthread_mutex_t _as_lock;

    int _reset_t;
    int *_is_blk_reseted;

    int *_is_time_updated;       // whether the set calculated at time t is updated

    int *_time_exw_count;
    pthread_mutex_t _time_exw_count_lock;

    int *_time_grad_count;
    pthread_mutex_t _time_grad_count_lock;

    sem_t *_time_pkg_receive;
    // sem_t *_time_pkg_updt;

    /**
     * buffer
     */
    BufferPool<grad_t> _send_buf;
    double **_recv_buf;

    /**
     * computational thread
     */
    CompClient *_cclients;
    sem_t _compclient_init_sem;
    sem_t _compclient_start_sem;
    pthread_t *_compclientp;

    /**
     * time statistics
     */
    double *_comp_time,  _comp_time_min, _comp_time_max, _objv_time;

    // double _net_time;

    /**
     * training data
     */
    ix_t _n, _p, _global_p;                // #sample, #feature
    int _is_binary_feature;                   // binary feature ..

    double   *_Y;
    double   *_val;
    uint32_t *_idx;
    uint64_t *_cnt;
    // double *_blk_row_mean;
    // double *_blk_row_max;

    bitmap_t **_nonempty_fea;                 // whether a fea contains nonzeros entries
    int *_local_feaid;
    bitmap_t *_fea_map;

    /**
     merge related var
    */
     std::set<int> _grad_inited_t;
     std::map<int, pthread_mutex_t *> _grad_merge_lock; // time t's merge lock
     pthread_mutex_t _grad_inited_t_lock;
};


struct server_t {
    ix_t p, *client_p;
    double *w, *delta;
    bitmap_t *as, **client_as;
    ix_t nas;
    int id;
    grad_t *recv_buf;
    double **send_buf;
};

class ServerProc {
        DECLARE_SINGLETON(ServerProc);

public:
    int Init(parameter_t *param);
    int UpdtW();

    inline double GetObjv() {
        double objv = 0;
        for (int i = 0; i < _N; i++) {
            for (ix_t j = 0; j < _blks[i].p; j++) {
                objv += _lambda * fabs(_blks[i].w[j]);
            }
        }
        return objv;
    }

    inline ix_t GetNNZW() {
        ix_t nnz = 0;
        for (int i = 0; i < _N; i++) {
            for (ix_t j = 0; j < _blks[i].p; j++) {
                if (_blks[i].w[j] != 0) {
                    ++ nnz;
                }
            }
        }
        return nnz;
    }

    inline ix_t GetNNZAS() {
        ix_t nnz = 0;
        for (int i = 0; i < _N; i++) {
            // nnz += bitmap_nnz(_blks[i].as);
            nnz += _blks[i].nas;
        }
        return nnz;
    }

    inline double GetShrk() {
        return _shrk;
    }

    int ResetActiveset(int reshrk, int reset_time);

private:
    int MergeGrad(server_t *blk, grad_t *recv, ix_t n, int client);
    int CalcDelta(server_t *blk);
    int UpdtAs(server_t *blk, int t);

    server_t *_blks;

    double _shrk,  _shrk_new, _alpha, _lambda;

    int _rank_size, _my_rank;
    int _N;                    // number of sets this server maintains
    ix_t _p;                    // # of feature this server maintains
    ix_t _total_p;              // total # of features
    int _H_type, _shrk_on, _delay;

    ix_t *_col_cut;
    int *_blk_iter_order;

    // double *_blk_row_mean;
    // double *_blk_row_max;

    double *_send_buf;

    pthread_mutex_t _as_lock;           /* lock the following 4 */
    int *_pkg_count;
    int _reset_t;
    int *_is_blk_reseted;

};

}
#endif

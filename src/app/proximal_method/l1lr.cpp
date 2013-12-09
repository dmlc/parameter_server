#include "l1lr.h"

namespace discd {

IMPLEMENT_SINGLETON(ClientProc);

int ClientProc::Iter() {
    Talk *talk = Talk::instance();
    pkg_t pkg;
    // timeval tv;

    int    reshrk     = 0;
    double reshrk_eps = _stop_eps * 1e2;

    /**
     * let compclient start
     */
    for (int i = 0; i < _thread_num; i++) {
        sem_post(&_compclient_start_sem);
    }

    ASSERT(!CalcObjv(), "calc object value fail");
    double objv_old = _objv;
    if (_my_rank == 0) {
        printf("  \t  T         objv  rel objv    nnz fea     nnz as      shrk   comp   total\n");
        printf("  \tinit%12.5e     -     %10lu %10lu %9.2e   -      -      %6.2f   \n",
               _objv, _nnz_w, _nnz_as, ServerProc::instance()->GetShrk(), _objv_time);
    }

    timeval tv;
    tic(tv);
    for (int t = 0; t < _max_t+1; t++) {

        if (t < _max_t) {
            talk->RecvUpdt(&pkg);
        } else {
            for (int i = 0; i < _thread_num; i++) {
                ASSERT(!pthread_join(_compclientp[i], NULL));
            }
        }

        /**
         * finished one pass of data, (though maybe not exaclty), measure the progress
         */
        if (t > 0 && (t%_N) == 0) {
            int it = t / _N;
            ASSERT(!CalcObjv(), "calc object value failed");
            double rel = (objv_old-_objv)/_objv;
            objv_old = _objv;
            if (_my_rank == 0) {
                printf("T:\t%3d %12.5e %9.2e %10lu %10lu %9.2e %6.2f %6.2f %6.2f %6.2f\n",
                       it, _objv, rel, _nnz_w, _nnz_as, ServerProc::instance()->GetShrk(),
                       _comp_time_min, _comp_time_max, _objv_time,  toc(tv));
                tic(tv);
            }
            /**
             * finished iterations
             */
            if ( t == _max_t || (fabs(rel) < _stop_eps && t > 3*_N && (!_shrk_on || reshrk))) {
                break;
            }
            /**
             * shrinkage next iteration?
             */
            // if (!_shrk_on)
            //     continue;

            reshrk = 0;
            if (fabs(rel) < reshrk_eps) {
                reshrk = 1;
                reshrk_eps = max(reshrk_eps*.1, _stop_eps);
                ASSERT(!ResetActiveset(t+_delay+1));
            }
            ASSERT(!ServerProc::instance()->ResetActiveset(reshrk, t+_delay+1));
        }

        /**
         *  unpack the package
         */
        int  recv_t       = pkg.time;
        int  bid          = _blk_iter_order[recv_t];
        ix_t n            = (ix_t)(pkg.buf_len/sizeof(double));
        _recv_buf[recv_t] = (double *)pkg.buf;

        _send_buf.Delete(recv_t);

        ASSERT(n==_blks[bid].nas, "I [%d] send [%u] coords to [%d] at time [%d], but only [%u] returned",
               _my_rank, _blks[bid].nas, pkg.orig, recv_t, n);

#ifdef _DEBUG_
        DEBUG("t %3d, C[%d] <<< S[%d], nnz [%d]", pkg.time, _my_rank, pkg.orig, n);
#endif

        /**
         * update activeset and delta
         */
        ASSERT(!UpdtAsDelta(recv_t));

        /**
         * tell compclient to do update
         */
        for (int i = 0; i < _thread_num; i++) {
            sem_post(_time_pkg_receive+recv_t);
        }

        /**
         * no need to wait them done
         */
        // for (int i = 0; i < _thread_num; i++) {
        //     sem_wait(_time_pkg_updt+recv_t);
        // }

        // AfterUpdtExw(recv_t);
        // talk->FreePkg(&pkg);

        // tic(start);
        // local->ResetTimeStatics();         // record some cpu_times
    }
    return 0;
}

int ClientProc::CalcObjv() {
    double local_v[3];
    double global_v[3];
    ServerProc *sp = ServerProc::instance();

    timeval tv;
    tic(tv);
    local_v[1] = (double) sp->GetNNZW();
    local_v[2] = (double) sp->GetNNZAS();
    local_v[0] = GetObjv() + sp->GetObjv();

    ASSERT(!MPI_Allreduce(local_v, global_v, 3, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD),
           "mpi_allreduce failed");

    _objv   = global_v[0];
    _nnz_w  = (uint64_t) global_v[1];
    _nnz_as = (uint64_t) global_v[2];
    _objv_time = toc(tv);

    // time
    double *all_comp_time;
    NEW_SET0(all_comp_time, double, _thread_num*_rank_size);

    ASSERT(!MPI_Gather(_comp_time, _thread_num, MPI_DOUBLE,
                          all_comp_time, _thread_num, MPI_DOUBLE, 0,
                          MPI_COMM_WORLD));
    for (int i = 0; i < _thread_num; i++)
        _comp_time[i] = 0;

    if (_my_rank == 0) {
        char filename[1024];
        snprintf(filename,1024,"./client_time");
        FILE *fd = fopen(filename,"a");
        if (fd != NULL) {
            gettimeofday(&tv, NULL);
            fprintf(fd, "%lu", tv.tv_sec*1000000+tv.tv_usec);
            for (int i = 0; i < _thread_num*_rank_size; i++) {
                fprintf(fd, "  %.3f", all_comp_time[i]);
            }
            fprintf(fd, "\n");
            fclose(fd);
        }

        _comp_time_min = 1e8;
        _comp_time_max = 0;
        for (int i = 0; i < _thread_num*_rank_size; i++) {
            _comp_time_max = max(_comp_time_max, all_comp_time[i]);
            _comp_time_min = min(_comp_time_min, all_comp_time[i]);
        }
    }
    return 0;
}

int ClientProc::ResetActiveset(int reset_time)
{
    if (!_shrk_on)
        return 0;

    pthread_mutex_lock(&_as_lock);
    _reset_t = reset_time;
    for (int i = 0; i < _N; i++) {
        _is_blk_reseted[i] = 1;
    }

    /**
     * do not reset the blk hasn't updated exw and as yet,
     * which will be postponded in AfterUpdtExw()
     */
    for (int t = _reset_t - _delay - 1; t < _reset_t; t++) {
        if (!_is_time_updated[t]) {
            _is_blk_reseted[_blk_iter_order[t]] = 0;
        }
    }

    for (int i = 0; i < _N; i++) {
        if (_is_blk_reseted[i]) {
            bitmap_reset1(_blks[i].as);
            _blks[i].nas = _blks[i].p;
        }
    }
    pthread_mutex_unlock(&_as_lock);
    return 0;
}


/**
 * update Delta and activeset, and packing the recv_buf
 */
int ClientProc::UpdtAsDelta(int t)
{

    int     bid   = _blk_iter_order[t];
    client_t *blk  = _blks + bid;

    double *D     = _recv_buf[t];

    pthread_mutex_lock(&_as_lock);
    ix_t c1 = 0, c2 = 0;
    for (ix_t j = 0; j < blk->p; j++) {
        if (!bitmap_test(j, blk->as)) {
            continue;
        }
        double d = D[c1++];
        if (d > 100) {
            bitmap_clr(j, blk->as);
            blk->nas --;
            d = 0;
        } else {
            D[c2++] = d;
        }
        blk->delta[j] = 2 * fabs(d) + .1;
    }
    pthread_mutex_unlock(&_as_lock);

#ifdef _DEBUG_
    DEBUG("t %3d, r [%d] blk [%d], bitmap nnz [%u] after updt exw",
          t, _my_rank, bid, blk->nas);
#endif

    return 0;
}


int ClientProc::Invoke(int tid) {
    ASSERT(!_cclients[tid].Init(tid));
    return 0;
}

void *CompClientThread(void *args) {
    int tid = (int)(int64_t) args;
    if (ClientProc::instance()->Invoke(tid) != 0)
        exit(-1);
    return NULL;
}

 int ClientProc::Init(parameter_t *param)
 {
     _rank_size   = param->rank_size;
     _my_rank     = param->my_rank;
     _N           = param->N;
     _thread_num  = param->thread_num;
     _delay       = param->delay;
     _H_type      = param->H;
     _stop_eps    = param->stop;
     _max_t       = _N * param->max_iter;
     _shrk_on     = param->shrk_on;
     _reset_t     = 0;

     /**
      * load data, each nodes read a part of rows
      */
     ASSERT(ReadProblem(param)==0, "read problem failed");

     /**
      * map the global feaid into local
      */
    _global_p = _p;
     NEW_SET0(_local_feaid, int, _global_p);
     NEW(_nonempty_fea, bitmap_t*, _N);

     _p = 0;
     _col_cut = linspace<ix_t>(0, _global_p, _N);

     for (int b = 0; b < _N; b++) {
         ix_t s = _col_cut[b];
         ASSERT(bitmap_new(_col_cut[b+1]-s, _nonempty_fea+b)==0, "no mem");
         for (ix_t i = s; i < _col_cut[b+1]; i++) {
             if (bitmap_test(i, _fea_map)) {
                 _local_feaid[i] = _p++;
                bitmap_set(i-s, _nonempty_fea[b]);
             } else {
                 _local_feaid[i] = -1;
             }
         }
     }
     DELETE(_fea_map);

     /**
      * map _col_cut to local_feaid
      * TODO _col_cut can be set by custom
      */
     _max_blk_p = 0;
     for (int i = 0; i < _N; i++) {
         ix_t *v = _col_cut + i + 1;
         for (ix_t j = *v; j > *(v-1); j--) {
             if (_local_feaid[j-1] >= 0) {
                 *v = _local_feaid[j-1] + 1;
                 break;
             }
         }
         _max_blk_p = max(_max_blk_p, *v - *(v-1));
     }
     ASSERT(_col_cut[_N] == _p, "_col_cut error");

     /**
      * each thread (CompClient) works on a part of rows
      */
     _row_cut = linspace<ix_t>(0, _n, _thread_num);

     /**
      * let each compclient do initialization
      */
     pthread_mutex_init(&_grad_inited_t_lock, NULL);
     _cclients = new CompClient[_thread_num];
     sem_init(&_compclient_init_sem, 0, 0);
     sem_init(&_compclient_start_sem, 0, 0);

     NEW(_compclientp, pthread_t, _thread_num);
     for (int i = 0; i < _thread_num; i++) {
         ASSERT(!pthread_create(_compclientp+i, NULL, CompClientThread, (void*)(int64_t)i));
     }

     /**
      * waiting each comclient finished initilization
      */
     for (int i = 0; i < _thread_num; i++) {
         sem_wait(&_compclient_init_sem);
     }

     /**
      * delete something,
      */
     DELETE(_local_feaid);
     if (!_is_binary_feature) {
         DELETE(_Y);
         DELETE(_val);
     }

     DELETE(_cnt);
     DELETE(_idx);

     // print debug information

     char filename[1024];
     snprintf(filename,1024,"./blkinfo.%d",_my_rank);
     FILE *fd = fopen(filename,"w");
     if (fd != NULL) {
         for (int b = 0; b < _N; b++) {
             fprintf(fd, "%u", _col_cut[b+1]-_col_cut[b]);
             for (int i = 0; i < _thread_num; i++) {
                 CompClient *c = _cclients+i;
                 fprintf(fd, "  %lu", c->_cnt[_col_cut[b+1]]-c->_cnt[_col_cut[b]]);
             }
             fprintf(fd, "\n");
         }
         fclose(fd);
     }

     /**
      * new activeset, delta....
      */

     ASSERT(!pthread_mutex_init(&_as_lock, NULL));

     NEW(_blks, client_t, _N);
     for (int i = 0; i < _N; i++) {
         client_t *blk = _blks + i;
         blk->p = _col_cut[i+1]-_col_cut[i];
         /**
          * new activeset
          */
         ASSERT(!bitmap_new(blk->p, &blk->as), "no mem for as");
         bitmap_reset1(blk->as);
         blk->nas = blk->p;

         /**
          * new delta
          */
         NEW(blk->delta, double, blk->p);
         for (ix_t i = 0; i < blk->p; i++) {
             blk->delta[i] = param->Delta_init;
         }
     }

    /**
     * for reactivation
     */
    NEW(_is_blk_reseted, int, _N);
    NEW_SET0(_is_time_updated, int, _max_t);

    /**
     * which server holds blk i
     */
    NEW(_blk_server, int, _N);
    for (int i = 0; i < _N; i ++) {
        _blk_server[i] = i % _rank_size;
    }

    /**
     * send buffer
     */
    _send_buf.Init(_max_t, _delay+1, _max_blk_p+PKGHEADERSIZE);


    /**
     * tracking progress at each time
     */
    NEW_SET0(_time_grad_count, int, _max_t);
    NEW_SET0(_time_exw_count, int, _max_t);
    pthread_mutex_init(&_time_grad_count_lock, NULL);
    pthread_mutex_init(&_time_exw_count_lock, NULL);
    // NEW(_time_pkg_updt, sem_t, _max_t);
    NEW(_time_pkg_receive, sem_t, _max_t);
    for (int i = 0; i < _max_t; i++) {
        // sem_init(_time_pkg_updt+i, 0, 0);
        sem_init(_time_pkg_receive+i, 0, 0);
    }
    NEW_SET0(_recv_buf, double *, _max_t);

    /**
     * the random passing order of blocks
     */
    srand(param->seed);
    NEW(_blk_iter_order, int, _max_t);
    if (_my_rank == 0) {
        for (int t = 0; t < param->max_iter; t++) {
            int *p = _blk_iter_order + t*_N;
            if (t == 0) {
                /**
                 * time 0, just random
                 */
                for (int i = 0; i < _N; i++) {
                    p[i] = i;
                }
                randperm<int>(p, _N);
            } else {
                /**
                 * the same blocks should not be updated more than once within _delay time
                 */
                for (int i = 0; i < _N; i++)
                    p[i] = -1;
                for (int i = 0; i < _N; i++) {
                    int m = rand() % (_N-max(_delay,i));
                    for (int j = max(_delay-i,0); j < _N; j++) {
                        if (p[j] <0) {
                            if (m==0) {
                                p[j] = p[-i-1];
                                break;
                            } else {
                                -- m;
                            }
                        }
                    }
                }
            }
        }
        /**
         * double check
         */
        int *p = _blk_iter_order;
        for (int i = 0; i < _max_t; i++) {
            for (int j = max(0,i-_delay);  j < i; j ++) {
                ASSERT(p[i] != p[j], "%d %d", i, j);
            }
        }
    }
    /**
     * broadcast from rank 0
     */
    ASSERT(!MPI_Bcast(_blk_iter_order, _max_t, MPI_INT, 0, MPI_COMM_WORLD), "bcast failed");

    NEW_SET0(_comp_time, double, _thread_num);
    return 0;
}

char lblname[1024], cntname[1024], valname[1024], idxname[1024], sizename[1024], mapname[1024];
int ClientProc::ReadProblem(parameter_t *param) {


    snprintf(lblname, 1024, "%s.label", param->data);
    snprintf(cntname, 1024, "%s.rowcnt", param->data);
    snprintf(valname, 1024, "%s.value", param->data);
    snprintf(idxname, 1024, "%s.colidx", param->data);
    snprintf(sizename, 1024, "%s.size", param->data);

    _Y = NULL;
    _val = NULL;
    _cnt = NULL;
    _idx = NULL;

    /**
     * get the total #samples
     */
    struct stat file_stat;
    ASSERT(!stat(lblname, &file_stat), "stat %s failed", lblname);

    size_t global_n = file_stat.st_size/sizeof(double);
    if (param->n == 0 || param->n > global_n) {
        param->n = global_n;
    }

    /**
     * check whether there is xxx.size
     */
    _p = 0;
    ix_t tmp_n;
    if (!stat(sizename, &file_stat)) {
        FILE *fd = fopen(sizename, "r");
        ASSERT(fd!=NULL, "fopen failed");
        ASSERT(fscanf(fd, "%u %u", &tmp_n, &_p)==2, "read %s error", sizename);
        fclose(fd);
        ASSERT(tmp_n == (ix_t)global_n, "%s didn't match %s", sizename, lblname);
    }

    /**
     * each node read a part of rows
     */
    size_t *row_os = linspace((size_t)0, param->n, _rank_size);
    size_t start = row_os[_my_rank];
    size_t n = row_os[_my_rank+1] - start;

    /**
     * first check whether there is a cache
     */
    int hit_cache = 0;
    if (param->cache_data != NULL) {
        snprintf(mapname, 1024, "%s.map.%d", param->cache_data, _my_rank);
        char tmpname[1024];
        snprintf(tmpname, 1024, "%s.label.%d", param->cache_data, _my_rank);
        if (!stat(tmpname, &file_stat)) {
            size_t nlbl = file_stat.st_size/sizeof(double);
            if (nlbl == (size_t)n) {
                hit_cache = 1;
                start = 0;
                snprintf(lblname, 1024, "%s.label.%d", param->cache_data, _my_rank);
                snprintf(cntname, 1024, "%s.rowcnt.%d", param->cache_data, _my_rank);
                snprintf(valname, 1024, "%s.value.%d", param->cache_data, _my_rank);
                snprintf(idxname, 1024, "%s.colidx.%d", param->cache_data, _my_rank);
                NOTICE("C[%d], hit training data cache", _my_rank);
            }
        }
    }

// #ifdef _DEBUG_
//     NOTICE("r [%d], row [%lu]->[%lu], entry [%lu]->[%lu]",
//            _my_rank, start, start+n, _cnt[0], _cnt[n]);
// #endif

    /**
     * read
     */
    ASSERT(load_bin(lblname, &_Y, start, n)>0, "load Y failed");
    ASSERT(load_bin(cntname, &_cnt, start, n+1)>0, "load cnt failed");

    uint64_t nnz = _cnt[n] - _cnt[0];
    if (!hit_cache) {
        start = _cnt[0];
    }
    ASSERT(load_bin(idxname, &_idx, start, nnz)>0, "load idx failed");

    _is_binary_feature = 1;
    if (!stat(valname, &file_stat)) {
        ASSERT(load_bin(valname, &_val, start, nnz)>0, "load val failed");
        /**
         * check whether it is binary feature
         */
        for (size_t i = 0; i < nnz; i++)
            if (_val[i] != 1) {
                _is_binary_feature = 0;
                break;
            }
        if (_is_binary_feature)
            DELETE(_val);
    }

    if (_is_binary_feature && !_my_rank) {
        NOTICE("we have binary features");
    }

    /**
     * get #samples, #features
     */
    _n = (uint32_t) n;
    if (_p == 0) {
        ix_t tmp = 0;
        for (size_t i = 0; i < nnz; i++) {
            tmp = max(tmp, _idx[i]);
        }
        ++ tmp;

        ASSERT(!MPI_Allreduce(&tmp, &_p, 1, MPI_UNSIGNED, MPI_MAX, MPI_COMM_WORLD),
               "mpi allreaduce failed");
        if (_my_rank == 0) {
            NOTICE("#samples [%lu], #features [%u] (by scanning data), #nodes [%d]", param->n, _p, _rank_size);
        }

    } else {
        if (_my_rank == 0) {
            NOTICE("#samples [%lu], #features [%u] (by reading .size), #nodes [%d]", param->n, _p, _rank_size);
        }
    }

    /**
     * store caches
     */
    if (param->cache_data != NULL && !hit_cache) {
        snprintf(lblname, 1024, "%s.label.%d", param->cache_data, _my_rank);
        snprintf(cntname, 1024, "%s.rowcnt.%d", param->cache_data, _my_rank);
        snprintf(valname, 1024, "%s.value.%d", param->cache_data, _my_rank);
        snprintf(idxname, 1024, "%s.colidx.%d", param->cache_data, _my_rank);

        ASSERT(save_bin(lblname, _Y, n)>0, "save cache Y failed");
        ASSERT(save_bin(cntname, _cnt, n+1)>0, "save cache cnt failed");
        ASSERT(save_bin(idxname, _idx, nnz)>0, "save cache idx failed");

        if (!_is_binary_feature) {
            ASSERT(save_bin(valname, _val, nnz)>0, "save cache val failed");
        }
        NOTICE("C[%d], save data cache in %s", _my_rank, param->cache_data);
    }

    /**
     * minus the offset
     */
    os_t os = _cnt[0];
    for (ix_t i = 0; i <= _n; i++)
        _cnt[i] -= os;


    /**
     * whether a feature exists in this rank
     */

    ASSERT(bitmap_new(_p, &_fea_map)==0);
    if (hit_cache) {
        if (!stat(mapname, &file_stat)) {
            size_t nmap = file_stat.st_size/sizeof(bitmap_t);
            if (nmap == (size_t)bitmap_size(_fea_map)) {
                NOTICE("C[%d] hit bitmap", _my_rank);
                ASSERT(load_bin(mapname, &_fea_map)>0);
                return 0;
            }
        }
    }

    for (size_t i = 0; i < nnz; i++) {
        ix_t j = _idx[i];
        ASSERT(j < _p, "too large idx [%u], while _p [%u]", j, _p);
        if (!bitmap_test(j, _fea_map))
            bitmap_set(j, _fea_map);
    }

    if (param->cache_data != NULL) {
        ASSERT(save_bin(mapname, _fea_map, bitmap_size(_fea_map))>0);
    }

    return 0;
}

// /**
//  * statistic informations
//  */
// double *local_mean, *local_max;

// NEW_SET0(_blk_row_mean, double, _N);
// NEW_SET0(_blk_row_max, double, _N);
// NEW_SET0(local_mean, double, _N);
// NEW_SET0(local_max, double, _N);

// for (int s = 0; s < _N; s++) {
//     for (ix_t i = 0; i < _n; i++) {
//         ix_t n = _data_blks[s].row_os[i+1] - _data_blks[s].row_os[i];
//         local_mean[s] += (double) n;
//         local_max[s] = max(local_max[s], (double)n);
//     }
// }

// ASSERT(!MPI_Allreduce(local_mean, _blk_row_mean, _N, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD), "allreduce..");
// ASSERT(!MPI_Allreduce(local_max, _blk_row_max, _N, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD), "allreduce..");

// for (int s = 0; s < _N; s++) {
//     _blk_row_mean[s] /=  (double)_rank_size* (double)_n;
// }



IMPLEMENT_SINGLETON(ServerProc);

int ServerProc::ResetActiveset(int reshrk, int reset_time)
{
    if (!_shrk_on)
        return 0;

    if (!reshrk) {
        double g_shrk_new;
        if (MPI_Allreduce(&_shrk_new, &g_shrk_new, 1,
                   MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD) != 0) {
            FATAL("mpi_allreduce failed");
            return -1;
        }
        _shrk = g_shrk_new / (double) _total_p * 10;
        _shrk_new = 0;
        return 0;
    }

    // DEBUG("reset server time");

    _shrk = 1e20;
    _shrk_new = 0;

    pthread_mutex_lock(&_as_lock);
    _reset_t = reset_time;
    for (int i = 0; i < _N; i++) {
        _is_blk_reseted[i] = 1;
    }
    for (int t = _reset_t-_delay; t < _reset_t; t++) {
        int s = _blk_iter_order[t];
        if (_pkg_count[t] != -1 && s >= 0) {
            _is_blk_reseted[s] = 0;
        }
    }
    for (int i = 0; i < _N; i++) {
        if (_is_blk_reseted[i]) {
            bitmap_reset1(_blks[i].as);
            _blks[i].nas = _blks[i].p;
        }
    }
    // prvec<int>(_is_set_reseted, _N);

    pthread_mutex_unlock(&_as_lock);

    return 0;
}

void *UpdtWThread(void *args) {
    if (ServerProc::instance()->UpdtW() != 0)
        exit(-1);
    return NULL;
}

int ServerProc::Init(parameter_t *param)
{

    _rank_size = param->rank_size;
    _my_rank   = param->my_rank;
    _lambda    = param->lambda;
    _delay     = param->delay;
    _alpha     = param->alpha;
    _H_type    = param->H;
    _reset_t   = 0;
    _shrk      = 1e20;
    _shrk_new  = 0;
    _shrk_on   = param->shrk_on;

    int max_t  = param->max_iter * param->N;

    ClientProc *local = ClientProc::instance();

    /**
     * a server only maitain a part of blocks
     */
    int N = local->_N;
    ix_t *cc = linspace<ix_t>(0, local->_global_p, N);

    NEW_SET0(_col_cut, ix_t, N+1);
    NEW(_blk_iter_order, int, max_t);
    NEW(_blks, server_t, N);

    int *blk_map;               // map a client block id into server bid
    NEW(blk_map, int, N);
    // NEW(_blk_row_mean, double, N);
    // NEW(_blk_row_max, double, N);

    _N = 0;
    ix_t max_blk_p = 0;
    for (int i = 0; i < N; i++) {
        if (local->_blk_server[i] == _my_rank) {
            /**
             * yeah, i will maitain this block
             */
            ASSERT(cc[i+1] >= cc[i]);
            ix_t p         = cc[i+1] - cc[i];
            blk_map[i]     = _N;
            _col_cut[_N+1] = _col_cut[_N] + p;
            // _blk_row_max[_N]  = local->_blk_row_max[i];
            // _blk_row_mean[_N] = local->_blk_row_mean[i];

            /**
             * block info, w, delta
             */
            server_t *blk = _blks + _N;
            blk->p        = p;
            blk->id       = _N;
            max_blk_p     = max(max_blk_p, p);

            NEW_SET0(blk->w, double, p);
            NEW(blk->delta, double, p);
            for (ix_t i = 0; i < p; i++) {
                blk->delta[i] = param->Delta_init;
            }

            /**
             * buffers, send buffer will be newed later, TODO, use bufpool
             */
            NEW(blk->recv_buf, grad_t, p);
            NEW(blk->send_buf, double*, _rank_size);

            /**
             * activeset
             */
            ASSERT(bitmap_new(p, &blk->as)==0, "new as failed");
            bitmap_reset1(blk->as);
            blk->nas = p;

            NEW(blk->client_as, bitmap_t*, _rank_size);
            NEW(blk->client_p, ix_t, _rank_size);

            ++ _N;
        } else {
            blk_map[i] = -1;
        }
    }

    _p       = _col_cut[_N];
    _total_p = local->_global_p;

    for (int i = 0; i < max_t; i ++) {
        int s = local->_blk_iter_order[i];
        if (blk_map[s] >= 0) {
            _blk_iter_order[i] = blk_map[s];
        } else {
            _blk_iter_order[i] = -1;
        }
    }
    NEW(_is_blk_reseted, int, _N);
    NEW_SET0(_pkg_count, int, max_t);
    NEW(_send_buf, double, max_blk_p);

    /**
     * got bitmaps from all clients, communication by rings
     */
    size_t bsize = (_total_p>>BITMAP_SHIFT)*sizeof(bitmap_t) + N*8*sizeof(ix_t);
    char *recv_buf, *send_buf;
    NEW(recv_buf, char, bsize);
    NEW(send_buf, char, bsize);

    MPI_Status status;
    MPI_Request req;

    /**
     * the lenth of data this node should receive
     */
    size_t recv_len = 0;
    for (int i = 0; i < _N; i++) {
        recv_len += sizeof(ix_t) + bitmap_size(_blks[i].as)*sizeof(bitmap_t);
    }
    ASSERT(bsize > recv_len);

    for (int r = 0; r < _rank_size; r++) {
        int to_rank   = (_my_rank+_rank_size-r) % _rank_size;
        int from_rank = (_my_rank+r) % _rank_size;

        /**
         * packing the send buffer
         */
        size_t len = 0;
        for (int i = 0; i < N; i++) {
            if (local->_blk_server[i] != to_rank)
                continue;

            bitmap_t *b = local->_nonempty_fea[i];
            ix_t bp = cc[i+1]-cc[i];
            ASSERT(bp == bitmap_len(b),
                   "blk [%d], mismatch, [%u] != [%u]", i, bp, bitmap_len(b));

            /**
             * the nnz entry
             */
            ix_t bp2 = local->_blks[i].nas;
            *((ix_t*)(send_buf+len)) = bp2; // nnz in b
            len += sizeof(ix_t);

            /**
             * activeset
             */
            size_t l = bitmap_size(b)*sizeof(bitmap_t);
            memcpy(send_buf+len, b, l);
            len += l;
        }

        /**
         * send it
         */
        if (MPI_Isend((void *)send_buf, (int)len, MPI_CHAR, to_rank, 1, MPI_COMM_WORLD, &req) != 0) {
            FATAL("r[%d], send bitmap to rank [%d] failed", _my_rank, to_rank);
            return -1;
        }

        /**
         * receive
         */
        if (MPI_Recv((void *)recv_buf, (int)bsize, MPI_CHAR, from_rank, 1, MPI_COMM_WORLD, &status) != 0
            || status.MPI_ERROR != MPI_SUCCESS) {
            FATAL("r[%d], recv bitmap from rank [%d] failed", _my_rank, from_rank);
            return -1;
        }

        int l;
        ASSERT(MPI_Get_count(&status, MPI_CHAR, &l)==0, "get count failed");
        ASSERT(l == (int) recv_len, "r [%d] should get [%lu], but rank [%d] send me only [%d]",
               _my_rank, recv_len, from_rank, l);

        /**
         * unpacking
         */
        len = 0;
        for (int i = 0; i < _N; i++) {
            server_t *blk = _blks + i;
            ix_t nnz = *((ix_t *)(recv_buf+len));
            len += sizeof(ix_t);
            bitmap_t *b = (bitmap_t*)(recv_buf+len);
            ASSERT(nnz==bitmap_nnz(b),
                   "the header say there are [%u] nnz, but actual [%u] here", nnz, bitmap_nnz(b));

            /**
             * create and copy bitmap
             */
            size_t l = bitmap_size(b)*sizeof(bitmap_t);
            bitmap_t **b2 = blk->client_as+from_rank;

            ASSERT(bitmap_new(bitmap_len(b), b2)==0);
            memcpy(*b2, b, l);
            blk->client_p[from_rank] = nnz;
            // NOTICE("new bitmap blk [%d] %u", blk->id, bitmap_len(b));
            // ASSERT(nnz==bitmap_nnz(blk->client_as[from_rank]), "didn't match");

            // NOTICE("blk [%d] [%u] [%u] [%u]", i, (ix_t)((*b2)[bitmap_size(*b2)-6]), (ix_t)((*b2)[bitmap_size(*b2)-4]),  (ix_t)((*b2)[bitmap_size(*b2)-2]));
            NEW(blk->send_buf[from_rank], double, nnz+PKGHEADERSIZE);
            blk->send_buf[from_rank] += PKGHEADERSIZE;
            len += l;
        }

        ASSERT(len==recv_len, "unpacking error");

        /**
         * ensure the previous send is successed
         */
        if (MPI_Wait(&req, &status) != 0 || status.MPI_ERROR != MPI_SUCCESS) {
            FATAL("MPI wait failed");
            return -1;
        }
    }

    /**
     * free memories
     */
    DELETE(recv_buf);
    DELETE(send_buf);
    for (int i = 0; i < N; i++) {
        DELETE(local->_nonempty_fea[i]);
    }

    /**
     * lock and threads
     */
    ASSERT(pthread_mutex_init(&_as_lock, NULL)==0, "init lock failed");

    pthread_t globalp;
    if (pthread_create(&globalp, NULL, UpdtWThread, NULL) != 0 ) {
        FATAL("creat pthread failed!");
        return -1;
    }

    return 0;
}

int ServerProc::UpdtW()
{
    pkg_t pkg;
    Talk *talk = Talk::instance();

    while (1) {

        talk->RecvGrad(&pkg);

        /**
         * unpack the package and add the gradient into recv_buf
         */
        int       t      = pkg.time;
        int       bid    = _blk_iter_order[t];
        server_t *blk    = _blks + bid;
        int       client = pkg.orig;
        ix_t      n      = (ix_t) (pkg.buf_len/sizeof(grad_t));
        ASSERT(bid>=0, "GLOBAL\tr [%d] t[%d]: should not send blk [%d] to me", _my_rank, t, bid);
        ASSERT(n<=blk->client_p[client], "%u %u", n, blk->client_p[client]);

        if (_pkg_count[t] == 0) {
            memset(blk->recv_buf, 0, blk->p*sizeof(grad_t));
        } else {
            if (_pkg_count[t] < 0) {
                FATAL("GLOBAL\tr [%d] t[%d]: hi r [%d], why send me again?", _my_rank, t, pkg.orig);
                return -1;
            }
        }
        _pkg_count[t] ++;

        MergeGrad(blk, (grad_t *)pkg.buf, n, client);
        talk->FreePkg(&pkg);

#ifdef _DEBUG_
        DEBUG("t %3d, S[%d] <<< C[%d], blk [%d] nnz [%u]", t, _my_rank, pkg.orig, bid, n);
#endif

        if (_pkg_count[t] != _rank_size) {
            continue;
        }

        /**
         * received gradient from all clients, compute the updates
         */

        ASSERT(CalcDelta(blk)==0, "calc delta failed");;
        // ASSERT(bitmap_nnz(blk->client_as[0])==blk->client_p[0], "xxxx");

        /**
         * pack and send
         */
        for (int i = 0; i < _rank_size; i ++) {
            ix_t c = 0;
            bitmap_t *as = blk->client_as[i];
            double *D = blk->send_buf[i];
            // ASSERT(bitmap_nnz(as) == blk->client_p[i], "uuuuu");

            for (ix_t j = 0; j < blk->p; j++) {
                if (bitmap_test(j,blk->as) && bitmap_test(j,as)) {
                    D[c++] = _send_buf[j];
                }
            }

            // NOTICE("blk [%d] [%u] [%u] [%u]", blk->id, (ix_t)(as[bitmap_size(as)-6]),(ix_t)(as[bitmap_size(as)-4]), (ix_t)(as[bitmap_size(as)-2]));
            // ASSERT(bitmap_nnz(as) == blk->client_p[i], "zzzzz");
            talk->SendStatus(i, t, UPDT, (char *)D, c*sizeof(double));
        }

        /**
         * update the activeset
         */
        ASSERT(UpdtAs(blk, t)==0, "update activeset failed");

        /**
         * mark that we finished
         */
        _pkg_count[t] = -1;

    }

    return 0;
}

int ServerProc::MergeGrad(server_t *blk, grad_t *recv, ix_t len, int client) {
    bitmap_t *as = blk->client_as[client];
    ix_t c = 0;
    for (ix_t i = 0; i < blk->p; i++) {
        if (bitmap_test(i,blk->as) && bitmap_test(i,as)) {
            blk->recv_buf[i].G += recv[c].G;
            blk->recv_buf[i].U += recv[c].U;
            ++ c;
#ifdef _CHECK_
            ASSERT(c<=len);
#endif
        }
    }
    ASSERT(c==len, "blk [%d], should merge in [%u], but [%u] in actually",
           blk->id, len, c);
    // ASSERT(bitmap_nnz(as) == blk->client_p[client]);
    return 0;
}

int ServerProc::CalcDelta(server_t *blk)
{
    grad_t   *grad   = blk->recv_buf;
    double   *update = _send_buf;

    // int       bid    = blk->id;
    // double    eta    = _alpha*(1+(double)_delay)*max(_blk_row_mean[bid],1);
    double eta = _alpha;

    for (ix_t j = 0; j < blk->p; j++) {
        if (!bitmap_test(j, blk->as)) {
            continue;
        }

        double w  = blk->w[j];
        double g  = grad[j].G;
        double gp = g + _lambda;
        double gn = g - _lambda;

        /**
         * violation, used for shrinkage
         */
        if (_shrk_on) {
            double vio = 0;
            if (equal_zero(w)) {
                if (gp < 0) {
                    vio = - gp;
                } else if (gn > 0) {
                    vio = gn;
                } else if (gp > _shrk && gn < - _shrk) {
                    update[j] = 1024; // mark this feature will be shrinkaged soon
                    continue;
                }
            } else if (w > 0) {
                vio = fabs(gp);
            } else {
                vio = fabs(gn);
            }
            _shrk_new = max(_shrk_new, vio);
        }

        /**
         * update of w, soft-thresholding
         */
        double d  = - w;
        double u;
        // if (_H_type == 3) {
        //     _gg[i] = sqrt(_gg[i]*_gg[i]+g*g);
        //     u = _gg[i] * eta;
        // } else {
        u = grad[j].U * eta + 1e-20;
        // }

        if (gp <= u * w) {
            d = - gp / u;
        } else if (gn >= u * w) {
            d = - gn / u;
        }

        double l = min(blk->delta[j],10);

        d = min(l,max(-l,d));

        update[j] = d;
        blk->w[j] += d;
        blk->delta[j] = 2 * fabs(d) + .1;
    }
    return 0;
}

int ServerProc::UpdtAs(server_t *blk, int t) {

    if (!_shrk_on)
        return 0;

    double *update = _send_buf;

    pthread_mutex_lock(&_as_lock);

    /**
     * reset those bitmaps didn't reseted
     */
    if (t <  _reset_t && !_is_blk_reseted[blk->id]) {
        // FATAL("reset [%d]", t);
        bitmap_reset1(blk->as);
        _is_blk_reseted[blk->id] = 1;
        blk->nas = blk->p;
        pthread_mutex_unlock(&_as_lock);
        return 0;
    }

    for (ix_t j = 0; j < blk->p; j++) {
        if (!bitmap_test(j, blk->as)) {
            continue;
        }
        if (update[j] > 100) {
            bitmap_clr(j, blk->as);
            blk->nas --;
        }
    }

    pthread_mutex_unlock(&_as_lock);

#ifdef _DEBUG_
    DEBUG("t %3d, S [%d], blk [%d], after updt [%u] nnz, [%u]", t, _my_rank,blk->id,
          blk->nas, bitmap_nnz(blk->as));
#endif

    return 0;
}

/**
 * transpose the training matrix
 */
int CompClient::Transpose() {

    ix_t        start  = _cp->_row_cut[_pid];
    _n                 = _cp->_row_cut[_pid+1] - start;
    _p                 = _cp->_p;
    _is_binary_feature = _cp->_is_binary_feature;
    int       *map    = _cp->_local_feaid;
    size_t     *rcnt   = _cp->_cnt + start;
    uint32_t   *ridx   = _cp->_idx;
    size_t     nnz    = rcnt[_n] - rcnt[0];

    /**
     * scan once, to get the feature count
     */
    // DEBUG("start from %lu to %lu", rcnt[0], rcnt[_n]);
    NEW_SET0(_cnt, os_t, _p+1);
    for (size_t i = rcnt[0]; i < rcnt[_n]; i++) {
        int j = map[ridx[i]];
        ++ _cnt[j+1];
    }
    for (ix_t i = 0; i < _p; i++) {
        _cnt[i+1] += _cnt[i];
    }

    /**
     * fill Y, idx and val
     */
    NEW(_idx, ix_t, nnz);
    if (_is_binary_feature) {
        NEW(_Y, double, _n);
    } else {
        NEW(_val, double, nnz);
    }
    for (ix_t i = 0; i < _n; i ++) {
        double y = (double)_cp->_Y[start+i];
        if (_is_binary_feature) {
            _Y[i] = y;
        }
        for (os_t j = rcnt[i]; j < rcnt[i+1]; j++) {
            ix_t k = map[ridx[j]];
            if (!_is_binary_feature) {
                _val[_cnt[k]] = _cp->_val[j] * y;
            }
            _idx[_cnt[k]++] = i;
        }
    }

    /**
     * restore the cnt
     */
    ASSERT(_cnt[_p-1] == nnz, "tranpose error");
    for (ix_t i = _p; i > 0; i--) {
        _cnt[i] = _cnt[i-1];
    }
    _cnt[0] = 0;

    return 0;
}

int CompClient::Init(int pid) {
    _cp = ClientProc::instance();
    _pid = pid;

    ASSERT(!Transpose());

    NEW_SET0(_is_time_updated, int, _cp->_max_t);

    /**
     * intial w = 0, then exp(x'w) = 1
     */
    NEW(_exw, double, _n);
    for (ix_t i = 0; i < _n; i++) {
        _exw[i] = 1;
    }

    /**
     * send buff
     */
    ASSERT(!_send_buf.Init(_cp->_max_t, _cp->_delay+1, _cp->_max_blk_p+PKGHEADERSIZE));

    sem_post(&_cp->_compclient_init_sem);
    sem_wait(&_cp->_compclient_start_sem);
    ASSERT(!Iter());

    return 0;
}

int CompClient::Iter() {

    timeval tv;
    for (int t = 0; t < _cp->_max_t; t++) {
        /**
         * compute the block gradient for time t
         */
        tic(tv);
        ASSERT(!BlockGrad(t));
        _cp->_comp_time[_pid] += toc(tv);

        int dt = t - _cp->_delay;
        /**
         * first check whether there is any fast back package
         */
        for (int k = max(0,dt+1); k <= t; k++) {
            if (_is_time_updated[k])
                continue;
            int ret = sem_trywait(_cp->_time_pkg_receive+k);
            if (ret) {
                ASSERT(errno == EAGAIN);
                continue;
            }
            tic(tv);
            ASSERT(!BlockExw(k));
            _cp->_comp_time[_pid] += toc(tv);
        }
        /**
         * wait until received dt
         */
        if (dt < 0 || _is_time_updated[dt])
            continue;
        ASSERT(!sem_wait(_cp->_time_pkg_receive+dt));
        tic(tv);
        ASSERT(!BlockExw(dt));
        _cp->_comp_time[_pid] += toc(tv);
    }
    return 0;
}

int CompClient::AllFinished(int *count, pthread_mutex_t *lock, int t) {
    pthread_mutex_lock(lock);
    int *v = count + t;
    if (*v == _cp->_thread_num-1) {
        /**
         * all other threads has finished
         */
        *v = -1;
        pthread_mutex_unlock(lock);
        return 1;
    }
    /**
     * otherwise, just increase the count
     */
    (*v) ++;
    pthread_mutex_unlock(lock);
    return 0;
}

int CompClient::BlockGrad(int t) {

    int       bid   = _cp->_blk_iter_order[t];
    client_t *blk   = _cp->_blks + bid;
    grad_t   *grad  = _send_buf.Get(t);
    ix_t      start = _cp->_col_cut[bid];

    ASSERT(_cp->_col_cut[bid+1]-start==blk->p);

#ifdef _DEBUG_
    DEBUG("t %3d, calc_grad thread [%d], blk [%d]", t, _pid, bid);
#endif

    ix_t c = 0;
    for (ix_t i = 0; i < blk->p; i++) {
        if (!bitmap_test(i, blk->as))
            continue;

        double G = 0, U = 0;
        double d = blk->delta[i];
        double l = 1;
        if (_is_binary_feature) {
            l = exp(d);
        }
        for (os_t j = _cnt[start+i]; j < _cnt[start+i+1]; j++) {
            ix_t row = _idx[j];
            double tau = 1 / ( 1 + _exw[row] );

            double v = 0;
            if (_is_binary_feature) {
                v = _Y[row];
                U += min(tau*(1-tau)*l, 0.25);
            } else {
                v = _val[j];
                U += min(tau*(1-tau)*exp(fabs(d*v)), 0.25) * v * v;
            }

            G -= v * tau;
        }

        grad[c].G = G;
        grad[c].U = U;
        ++ c;
    }

    ASSERT(c==blk->nas, "t [%d], blk [%d], bitmap didn't match", t, bid);

    /**
     * merge the gradients
     */

    grad_t *G = _cp->_send_buf.Get(t) + PKGHEADERSIZE;
    size_t  size = blk->nas*sizeof(grad_t);
    pthread_mutex_t *merge_lock = NULL;
    pthread_mutex_lock(&_cp->_grad_inited_t_lock);
    if (_cp->_grad_inited_t.find(t) == _cp->_grad_inited_t.end()) {
        memset(G, 0, size);
        _cp->_grad_merge_lock[t] = new pthread_mutex_t[MERGE_LOCK_NUM];
        for (int i = 0; i < MERGE_LOCK_NUM; ++i) {
            pthread_mutex_init(&_cp->_grad_merge_lock[t][i], NULL);
        }
        _cp->_grad_inited_t.insert(t);
    }
    merge_lock = _cp->_grad_merge_lock[t];
    pthread_mutex_unlock(&_cp->_grad_inited_t_lock);

    grad_t *TG = _cp->_cclients[_pid]._send_buf.Get(t);
    ix_t merge_per_lock = blk->nas / MERGE_LOCK_NUM;
    for (int m = 0; m < MERGE_LOCK_NUM; ++m) {
        int i = (m + _pid) % MERGE_LOCK_NUM;
        ix_t start = merge_per_lock * i;
        ix_t end = merge_per_lock * (i+1);
        if (i == MERGE_LOCK_NUM - 1) {
            end = blk->nas;
        }
        pthread_mutex_lock(&merge_lock[i]);
        for (ix_t j = start; j < end; j++) {
            G[j].G += TG[j].G;
            G[j].U += TG[j].U;
        }
        pthread_mutex_unlock(&merge_lock[i]);
    }

    if (!AllFinished(_cp->_time_grad_count, &_cp->_time_grad_count_lock, t))
        return 0;

    delete _cp->_grad_merge_lock[t];
    /**
     * send it
     */
    int   to_rank = _cp->_blk_server[bid];
    Talk::instance()->SendStatus(to_rank, t, GRAD, (char *)G, size);

    return 0;
}

int CompClient::BlockExw(int t) {
    int       bid   = _cp->_blk_iter_order[t];
    ix_t      start = _cp->_col_cut[bid];
    client_t *blk   = _cp->_blks + bid;
    double   *D     = _cp->_recv_buf[t];

    ASSERT(_cp->_col_cut[bid+1]-start==blk->p);
#ifdef _DEBUG_
    DEBUG("t %3d, updt_exw thread  [%d], blk [%d]",  t,  _pid, bid);
#endif

    ix_t c = 0;
    for (ix_t i = 0; i < blk->p; i++) {
        if (!bitmap_test(i, blk->as))
            continue;
        double d = D[c++];
        if (d==0 || d > 100)
            continue;
        for (os_t j = _cnt[start+i]; j < _cnt[start+i+1]; j++) {
            ix_t row = _idx[j];

            double v = 0;
            if (_is_binary_feature) {
                v = _Y[row];
            } else {
                v = _val[j];
            }

            _exw[row] *= exp(d*v);      // can avoid too many exp here, but store d*v in a buff
        }
    }

    ASSERT(c==blk->nas, "t [%d], blk [%d], bitmap didn't match", t, bid);

   _is_time_updated[t] = 1;
    _send_buf.Delete(t);

    if (AllFinished(_cp->_time_exw_count, &_cp->_time_exw_count_lock, t)) {
        /**
         * delete the package
         */
        DELETE(_cp->_recv_buf[t]);

        /**
         * reset the activeset if necessary
         */
        if (!_cp->_shrk_on)
            return 0;

        pthread_mutex_lock(&_cp->_as_lock);
        if (t < _cp->_reset_t && !_cp->_is_blk_reseted[bid]) {
            // DEBUG("reset time %d", t);
            bitmap_reset1(blk->as);
            blk->nas = blk->p;
            _cp->_is_blk_reseted[bid] = 1;
        }
        _cp->_is_time_updated[t] = 1;
        pthread_mutex_unlock(&_cp->_as_lock);
    }

    return 0;
}


} // end of namespace discd

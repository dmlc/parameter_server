#include "common.h"
#include "l1lr.h"
//#include "globalp.h"
#include "talk.h"
#include <getopt.h>

 // #define _DEBUG_

using namespace discd;

void usage(char *self) {
    printf("Usage: %s\n"
           "\t -f dataset name, in binary format\n"
           "\t -c cache dir\n"
           "\t -n #samples will use\n"
           "\t -l lambda \t(dafault 1)\n"
           "\t -p num_threads \t(default 4)\n"
           "\t -d num_delay \t(default 1)\n"
           "\t -b num_block \t(default 50)\n"
           "\t -g control of learning rate, the smaller the faster, but may do not converge \t(default 1)\n"
           "\t -e stop_eps \t(default 1e-5)\n"
           "\t -T max_iter \t(default 100)\n"
           "\t -k whether or not open the shrinkage trick\n"
           , self);
    exit(1);
}

void showparam(parameter_t *p) {
    NOTICE("dataset [%s], lambda [%g], #block [%d], #delay [%d], #thread [%d], alpha[%g], H [%d], shrk on [%d], seed [%u]",
           p->data, p->lambda, p->N, p->delay, p->thread_num, p->alpha, p->H, p->shrk_on, p->seed) ;
    return;
}

int main(int argc, char *argv[]) {
    using namespace discd;

    // if (argc != 7) {
    //     printf("Distributed Coordinate Descent Solving L1 Logisitic Regression:\n");
    //     printf("\tlogic(Xw,Y) + lambda |w|_1\n");
    //     printf("Usage: ./discd data_name lambda #block #thread\n");
    //     return 0;
    // }

    ASSERT(!MPI_Init(&argc, &argv), "MPI_Init failed");

    /**
     * setting parameters
     */
    parameter_t param;

    param.net_port   = 10000;
    param.data       = NULL;
    param.lambda     = 1;
    param.N          = 50;
    param.thread_num = 1;
    param.delay      = 1;
    param.alpha      = 1;
    param.max_iter   = 20;
    param.stop       = 1e-5;
    param.Delta_init = 5;
    param.shrk_on = 1;
    param.seed = 0;
    param.H = 2;
    param.n = 0;
    param.cache_data  = NULL;

    ASSERT(!MPI_Comm_rank(MPI_COMM_WORLD, &param.my_rank), "get my_rank failed.");
    ASSERT(!MPI_Comm_size(MPI_COMM_WORLD, &param.rank_size), "get rank_size failed.");

    char c;
    while ((c = getopt (argc, argv, "f:l:p:T:e:d:b:g:H:V:n:c:s:k:")) != -1) {
        switch (c) {
            case 'f':
                param.data = new char[256];
                strncpy(param.data, optarg, 256);
                break;
            case 'c':
                param.cache_data = new char[256];
                strncpy(param.cache_data, optarg, 256);
                break;
            case 'n':
                param.n = (size_t) atoi(optarg);
                break;
            case 'l':
                param.lambda = atof(optarg);
                break;
            case 'b':
                param.N = atoi(optarg);
                break;
            case 'g':
                param.alpha = atof(optarg);
                break;
            case 'p':
                param.thread_num = atoi(optarg);
                break;
            case 'T':
                param.max_iter = atoi(optarg);
                break;
            case 'd':
                param.delay = atoi(optarg);
                break;
            case 'e':
                param.stop = atof(optarg);
                break;
            case 'H':
                param.H = atoi(optarg);
                break;
            case 's':
                param.seed = (unsigned int)atoi(optarg);
                break;
            case 'k':
                param.shrk_on = atoi(optarg);
                break;
            default:
                usage(argv[0]);
        }
    }

    ASSERT(param.delay < param.N, "too large delay");
    ASSERT(param.N < 5000, "too many blocks");

    if (param.data == NULL) {
        usage(argv[0]);
    }

    if (param.my_rank == 0)
        showparam(&param);


    /**
     * init
     */
    timeval tv;
    tic(tv);
    ASSERT(!Talk::instance()->Init(&param), "init talk failed")
    // DEBUG("talk inited.");
    ASSERT(!ClientProc::instance()->Init(&param), "init local process failed");
   // DEBUG("client inited.");

    ASSERT(!ServerProc::instance()->Init(&param), "init global process failed");
    if (param.my_rank == 0) {
        NOTICE("finished initialization in %.2f sec, start iterating...", toc(tv));
    }
    /**
     * do update
     */
    ASSERT(!ClientProc::instance()->Iter(), "iterate failed");

    /**
     * finished
     */
    ASSERT(!MPI_Finalize(), "MPI_Finalize failed");

    return 0;

}

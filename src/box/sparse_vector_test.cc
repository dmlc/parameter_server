/*
 * sparse_vector.cpp
 *
 *  Created on: 23/11/2013
 *      Author: polymorpher
 */

#include "box/sparse_vector.h"
#include "gtest/gtest.h"

using namespace PS;
TEST(SparseVector, Push) {
	Key dimension = 1e+5;
	Key numKeys = 10;
	std::default_random_engine randomGenerator;
	std::uniform_int_distribution<Key> KeyDistribution(0, dimension);
	std::uniform_real_distribution<double> ValDistribution(0, 0.1);
	randomGenerator.seed(time(NULL));

	FLAGS_my_type = "server";
	int my_rank = 1;
//  SparseVector<double> originalSv("original",dimension);
//  for(size_t i=0;i<numKeys;++i){
//	  originalSv[KeyDistribution(randomGenerator)]=ValDistribution(randomGenerator);
//  }
	SparseVector<double>::SVec aaa(dimension);
	for (size_t i = 0; i < numKeys; i++) {
		aaa.coeffRef(KeyDistribution(randomGenerator)) = ValDistribution(
				randomGenerator);

	}
	for (SparseVector<double>::SVecIter it(aaa); it; ++it) {

		printf("init key=%lld val=%lf\n", it.index(), it.value());
	}
	double originalNorm = 0;
	originalNorm = aaa.norm();
	printf("original norm=%lf\n", originalNorm);
	pid_t pid = fork();
	if (pid == 0) {
		FLAGS_my_type = "client";
		my_rank++;
		pid_t pid2 = fork();
		if (pid2 == 0) {
			my_rank++;
			FLAGS_my_rank++;
		}
	}
	FLAGS_num_client = 2;
	srand(time(0) + my_rank);

	int delay = 3;
	double actual_delay;

	SparseVector<double> sv("awesome", dimension);

	if (FLAGS_my_type == "client") {
		sv.SetMaxDelay(10000, delay);
		int n = 10;
		SyncFlag flag;
		flag.set_recver(NodeGroup::kServers);
		flag.set_type(SyncFlag::PUSH_PULL);
		flag.set_push_delta(true);
		flag.set_pull_delta(false);
		for (int i = 0; i < n; ++i) {
			sv.getLocalVector() += aaa;
//			printf("itera=%d--\n", i);
//			for (SparseVector<double>::SVecIter it(sv.getLocalVector()); it;
//					++it) {
//				printf("key=%lld val=%lf\n", it.index(), it.value());
//			}
//			printf("--\n");
			EXPECT_TRUE(sv.Push(flag).ok());
			actual_delay = FLAGS_num_client * i
					- sv.getLocalVector().norm() / originalNorm;
//      printf("%lf\n",sv.getLocalVector().norm());
//       LL << sv.SName() << " delay = "<< actual_delay;
			std::this_thread::sleep_for(microseconds(500 + rand() % 100));
		}

		// std::this_thread::sleep_for(seconds(1));
		for (int i = 0; i < 100; ++i) {
			EXPECT_TRUE(sv.Push(flag).ok());
			actual_delay = FLAGS_num_client * n
					- sv.getLocalVector().norm() / originalNorm;
//       LL << sv.SName() << " fin " <<  actual_delay;
		}
		EXPECT_LT(fabs(actual_delay), 0.001);
//    for(SparseVector<double>::SVecIter it(originalSv.getLocalVector());it;++it){
//    	double newval=sv[it.index()];
//    	double originalVal=it.value();
//    	double diff=fabs(newval-originalVal*n);
//    	if(diff>0.001){
//    		printf("%lf %lf diff=%lf\n",newval, originalVal*n, diff);
//    	}
//    	EXPECT_LT(fabs(diff), 0.001);
//    }
	} else {
		std::this_thread::sleep_for(seconds(3));
	}

	int ret;
	wait(&ret);
}

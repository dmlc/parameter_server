/*
 * sparse_vector.h
 *
 *  Created on: 23/11/2013
 *      Author: polymorpher
 */

#pragma once

#include "container.h"
#include "util/common.h"
#include "util/xarray.h"
#include "box/container.h"
#include "util/eigen3.h"

namespace PS {
//TODO: TEST
template <class V> class SparseVector: public Container {
public:
	typedef Eigen::SparseVector<V> SVec;
	typedef typename Eigen::SparseVector<V>::InnerIterator SVecIter;
private:
	SVec localVector;
	SVec remoteVector;
public:
	inline SVec& getLocalVector(){return localVector;}
	inline SVec& getRemoteVector(){return remoteVector;}
	SparseVector(string name, size_t size):
		Container(name, 0,size),
		localVector(size),remoteVector(size){
		Container::Init();
	}
	//virtual ~SparseVector();
	V& operator[](const Key& key){
		return localVector.coeffRef(key);
	}
	//TODO: do some smart thing about this
	const V& operator[](const Key& key) const{
		return localVector.coeffRef(key);
	}
	inline SparseVector<V>& operator+=(const SparseVector<V>& rhs){
		localVector+=rhs.localVector;
		return *this;
	}


	Status GetLocalData(Mail *mail){
//		localVector.makeCompressed();
//		remoteVector.makeCompressed();
      if(mail->flag().push().delta()){
			size_t maximumChanges=localVector.nonZeros()+remoteVector.nonZeros();
			size_t counter0=0;
			for (SVecIter it(localVector);it;++it){
				if(it.value()!=remoteVector.coeff(it.index())){
					counter0++;
				}
			}
			for (SVecIter it(remoteVector);it;++it){
				if(it.value()!=0 && localVector.coeff(it.index())==0){
					counter0++;
				}
			}
			XArray<Key> pushKeys(counter0);
			XArray<V> pushValues(counter0);
			size_t counter=0;
			for (SVecIter it(localVector);it;++it){
				V remoteVal=remoteVector.coeff(it.index());
				if(it.value()!=remoteVal){
					pushKeys[counter]=it.index();
					pushValues[counter]=it.value()-remoteVal;
					counter++;
				}
			}
			for (SVecIter it(remoteVector);it;++it){
				if(it.value()!=0 && localVector.coeff(it.index())==0){
					pushKeys[counter]=it.index();
					pushValues[counter]=-it.value();
					counter++;
				}
			}
			CHECK_EQ(counter0,counter);
			//remove this stupid hotfix after downstream arrays /postoffices are fixed.
			if(counter==0){
				//printf("test222wtf\n\nn\n\n\n\n\n");
				XArray<Key> newPushKeys(1);
				XArray<V> newPushValues(1);
				newPushKeys[0]=pushKeys[0];
				newPushValues[0]=(V)0;
				mail->flag().mutable_key()->set_start(0);
				mail->flag().mutable_key()->set_end(1);
				mail->flag().mutable_key()->set_cksum(newPushKeys.raw().ComputeCksum());
				mail->set_keys(newPushKeys.raw());
				mail->set_vals(newPushValues.raw());

			}else{
//				pushKeys.resetSize(counter);
//				pushValues.resetSize(counter);

              mail->flag().mutable_key()->set_start(0);
              mail->flag().mutable_key()->set_end(counter);
              mail->flag().mutable_key()->set_cksum(pushKeys.raw().ComputeCksum());
				mail->set_keys(pushKeys.raw());
				mail->set_vals(pushValues.raw());
			}
		}else{
			size_t dataLength=localVector.nonZeros();
			CHECK_NE(dataLength,0);
			XArray<Key> pushKeys(dataLength);
			XArray<V> pushValues(dataLength);
			size_t counter=0;
			for(SVecIter it(localVector);it;++it){
				pushKeys[counter]=it.index();
				pushValues[counter++]=it.value();
			}
			CHECK_EQ(dataLength,counter);
			mail->flag().mutable_key()->set_start(0);
			mail->flag().mutable_key()->set_end(dataLength);
			mail->flag().mutable_key()->set_cksum(pushKeys.raw().ComputeCksum());
			mail->set_keys(pushKeys.raw());
			mail->set_vals(pushValues.raw());
		}
		remoteVector=localVector;
		return Status::OK();
	}
	Status MergeRemoteData(const Mail& mail){
		XArray<Key> keys(mail.keys());
		XArray<V> vals(mail.vals());
		CHECK_EQ(keys.size(), vals.size());
		if(mail.flag().push().delta()){
			for(size_t i=0;i<keys.size();i++){
				localVector.coeffRef(keys[i])+=vals[i];
				remoteVector.coeffRef(keys[i])+=vals[i];
			}
		}else{
			for(size_t i=0;i<keys.size();i++){
				localVector.coeffRef(keys[i])+=(vals[i]-remoteVector.coeffRef(keys[i]));
				remoteVector.coeffRef(keys[i])=vals[i];
			}
		}
		 LL << SName() << "merge: " << localVector.norm() << " " << remoteVector.norm();
//		localVector.makeCompressed();
//		remoteVector.makeCompressed();
		return Status::OK();
	}

};
template <class V> inline SparseVector<V> operator+(SparseVector<V> lhs, const SparseVector<V>& rhs){
	lhs+=rhs;
	return lhs;
}
} /* namespace PS */


//
//int main(){
//	Eigen::SparseVector<double> test(1000000000);
//	printf("%lf\n",test.coeff(5));
//	test.coeffRef(5)=10;
//	printf("%lf\n",test.coeff(5));
//	for(int c=0;c<100;c++){
//	for(int i=0;i<1000000000;i++){
//		int b=i;
//	}
//	}
//	printf("%zu",test.size());
//}

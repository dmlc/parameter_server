/*
 * sparsematrix.h
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
//TODO: test, consistency check, and improve fault tolerance.
template <class V> class SparseMatrix: public PS::Container {
	typedef Eigen::SparseMatrix<V> SMat;
	typedef typename Eigen::SparseMatrix<V>::InnerIterator SMatIter;
	typedef typename Eigen::SparseMatrixBase<V>::InnerVectorReturnType InnerVec;
	const static V _ZERO=(V)0;
	SMat localMatrix;
	SMat remoteMatrix;
	MatrixKey nRows;
	MatrixKey nCols;
private:
	inline Key pack(const MatrixKey& row, const MatrixKey& col){return nRows*col+row;}
	inline void unpack(const Key& index, MatrixKey* row, MatrixKey* col){*row=index%nRows;*col=index/nRows;}
public:
	SparseMatrix(MatrixKey numRows,MatrixKey numCols):
		localMatrix(numRows,numCols), remoteMatrix(numRows,numCols),
		nRows(numRows),nCols(numCols){

	}
	virtual ~SparseMatrix();
	V& getRef(const MatrixKey& row, const MatrixKey& col){
		return localMatrix.coeffRef(row,col);
	}
	const V& getRef(const MatrixKey& row, const MatrixKey& col) const{
		return localMatrix.coeffRef(row,col);
	}
	const V& get(const MatrixKey& row, const MatrixKey& col) const {
		return localMatrix.coeff(row,col);
	}
	InnerVec& operator[](const MatrixKey& col){
		//TODO: test and verify that innerVector(col) does indeed give back col-th column, instead
		//of some weird internal column index-th column
		//It is said in SparseBlock.h that this returns col-th column - but I cannot fully trust it.
		return localMatrix.innerVector(col);
	}
	const InnerVec& operator[](const MatrixKey& col) const{
		return localMatrix.innerVector(col);
	}
	inline SparseMatrix& operator+=(const SparseMatrix& rhs){
		localMatrix+=rhs.localMatrix;
		remoteMatrix+=rhs.remoteMatrix;
		return *this;
	}
	inline SparseMatrix operator+(SparseMatrix lhs, const SparseMatrix& rhs){
		lhs+=rhs;
		return lhs;
	}
	Status GetLocalData(Mail *mail){
		localMatrix.makeCompressed();
		remoteMatrix.makeCompressed();
			if(mail->flag().push_delta()){
				size_t maximumChanges=localMatrix.nonZeros()+remoteMatrix.nonZeros();
				XArray<Key> pushKeys(maximumChanges);
				XArray<V> pushValues(maximumChanges);
				size_t counter=0;
				for (int k=0;k<localMatrix.outerSize();++k){
					for (SMatIter it(localMatrix,k);it;++it){
						MatrixKey row=it.row();
						MatrixKey col=it.col();
						Key index=pack(row,col);
						if(it.value()!=remoteMatrix.coeff(row,col)){
							pushKeys[counter]=index;
							pushValues[counter++]=it.value();
						}
					}
				}
				for (int k=0; k<remoteMatrix.outerSize();++k){
					for (SMatIter it(remoteMatrix,k);it;++it){
						MatrixKey row=it.row();
						MatrixKey col=it.col();
						Key index=pack(row,col);
						if(it.value()==_ZERO && localMatrix.coeff(row,col)!=_ZERO){
							pushKeys[counter]=index;
							pushValues[counter++]=0;
						}
					}
				}
				pushKeys.resetSize(counter);
				pushValues.resetSize(counter);
				mail->flag().set_key_start(0);
				mail->flag().set_key_end(counter);
				mail->flag().set_key_cksum(pushKeys.raw().ComputeCksum()+pushValues.raw().ComputeCksum());
				mail->set_keys(pushKeys.raw());
				mail->set_vals(pushValues.raw());
			}else{
				size_t dataLength=localMatrix.nonZeros();
				XArray<Key> pushKeys(dataLength);
				XArray<V> pushValues(dataLength);
				size_t counter=0;
				for (int k=0; k<localMatrix.outerSize();++k){
					for(SMatIter it(localMatrix,k);it;++it){
						MatrixKey row=it.row();
						MatrixKey col=it.col();
						Key index=pack(row,col);
						pushKeys[counter]=index;
						pushKeys[counter++]=it.value();
					}
				}
				mail->flag().set_key_start(0);
				mail->flag().set_key_end(dataLength);
				mail->flag().set_key_cksum(pushKeys.raw().ComputeCksum());
				mail->set_vals(pushValues.raw());
			}
			remoteMatrix=localMatrix;
			return Status::OK();
		}
		Status MergeRemoteData(const Mail& mail){
			XArray<Key> keys(mail.keys());
			XArray<V> vals(mail.vals());
			if(mail.flag().push_delta()){
				for(size_t i=0;i<keys.size();++i){
					MatrixKey row;
					MatrixKey col;
					unpack(keys[i],&row,&col);
					localMatrix.coeffRef(row,col)+=vals[i];
					remoteMatrix.coeffRef(row,col)+=vals[i];
				}
			}else{
				for(size_t i=0;i<keys.size();++i){
					MatrixKey row;
					MatrixKey col;
					unpack(keys[i],&row,&col);
					localMatrix.coeffRef(row,col)=vals[i];
					remoteMatrix.coeffRef(row,col)=vals[i];
				}
			}
			localMatrix.makeCompressed();
			remoteMatrix.makeCompressed();
			return Status::OK();
		}
};

} /* namespace PS */


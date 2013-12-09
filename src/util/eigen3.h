#pragma once
#include <eigen3/Eigen/Sparse>
#include <eigen3/Eigen/Dense>
#include "util/common.h"
#include "util/bin_data.h"
#include "util/range.h"

namespace PS {

// sparse
typedef Eigen::SparseMatrix<double,Eigen::RowMajor> dsMat;
typedef Eigen::SparseMatrix<int,Eigen::RowMajor> isMat;
typedef Eigen::SparseMatrix<size_t,Eigen::RowMajor> psMat;
typedef Eigen::SparseVector<double> dsVec;

typedef Eigen::SparseMatrix<double,Eigen::RowMajor> DSMat;
typedef Eigen::Matrix<double, Eigen::Dynamic, 1> DVec;

// dense
typedef Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic,Eigen::ColMajor> dMat;
typedef Eigen::Matrix<double, Eigen::Dynamic, 1> dVec;
// typedef Eigen::Matrix<double, Eigen::Dynamic, 1> dVec;
typedef Eigen::Matrix<int, Eigen::Dynamic, 1> IVec;

// triplet
typedef Eigen::Triplet<double> dTri;
typedef Eigen::Triplet<int> iTri;
typedef Eigen::Triplet<size_t> pTri;

template <typename V>
inline void LoadVector(string const& name, Range<size_t> range, DVec *Y) {
  size_t n = range.size();
  V* vec = NULL;
  load_bin<V>(name, &vec, range.start(), n);
  Y->resize(n);
  for (size_t i = 0; i < n; ++i)
    Y->coeffRef(i) = vec[i];
  delete [] vec;
}

//  a quick reference:
// http://eigen.tuxfamily.org/dox/group__SparseQuickRefPage.html
// now i use the method in
// http://eigen.tuxfamily.org/dox/group__TutorialSparse.html sec:Filling a sparse matrix
// however, it will be better if you can memcpy the content directly using its
// low-level apis:
// sm1.valuePtr(); // Pointer to the values
// sm1.innerIndextr(); // Pointer to the indices.
// sm1.outerIndexPtr(); //Pointer to the beginning of each inner vector

// load a segment of row major binary data, and convert into eigen3
inline void LoadXY(string const& name, size_t start, size_t end, dVec *Y, dsMat *X) {
}

//   // check size
//   std::ifstream in(StrCat(name, ".size").c_str());
//   CHECK(in.good());
//   size_t row, col, nnz;
//   in >> row >> col >> nnz;
//   CHECK_EQ(bin_length<int32>(StrCat(name, ".lbl")), row);
//   CHECK_EQ(bin_length<uint64>(StrCat(name, ".cnt")), row + 1);
//   CHECK_GE(row, end);
//   CHECK_GE(start, 0);
//   CHECK_GE(end, start);
//   // load binary data
//   //row starts from 0, load row from [start, end),
//   long n = end - start;
//   uint64 *cnt = NULL;
//   double *val = NULL;
//   int32 *idx = NULL;
//   int32 *lbl = NULL;
//   load_bin<uint64>(StrCat(name, ".cnt"), &cnt, start, n+1);
//   load_bin<double>(StrCat(name, ".val"), &val, cnt[0], cnt[n]-cnt[0]);
//   load_bin<int32>(StrCat(name, ".idx"), &idx, cnt[0], cnt[n]-cnt[0]);
//   load_bin<int32>(StrCat(name, ".lbl"), &lbl, start, n);
//   // construct X
//   iVec reserve(n);
//   for (long i = 0; i < n; ++i)
//     reserve[i] = (int) (cnt[i+1] - cnt[i]);
//   X->resize(n, col);
//   X->reserve(reserve);
//   for (long i = 0; i < n; ++i)
//     for (size_t j = cnt[i]-cnt[0]; j < cnt[i+1]-cnt[0]; ++j)
//       X->insert(i, idx[j]) = val[j];
//   // LL << X->isCompressed();
//   X->makeCompressed();
//   // construct Y
//   Y->resize(n);
//   for (long i = 0; i < n; ++i)
//     Y->coeffRef(i) = lbl[i];
// }

// inline void FilldsMat(Vec const &lvalue, dsMat *rvalue) {
//   int n = lvalue.size();
//   CHECK_EQ(rvalue->nonZeros(), n);
//   double *val = rvalue->valuePtr();
//   for (int i = 0; i < n; ++i) {
//     val[i] = lvalue.coeff(i);
//   }
// }

// inline void FillVec(std::vector<double> const& lvalue, Vec *rvalue) {
//   rvalue->resize(lvalue.size(), 1);
//   for (size_t i = 0; i < lvalue.size(); ++i) {
//     rvalue->coeffRef(i) = lvalue[i];
//   }
// }
// // construct a sparse matrix from a vector of triplets
// template<class T>
// inline void FillDSMat(int n, int m, std::vector<Eigen::Triplet<T> > const &coef,
//                         Eigen::SparseMatrix<T,Eigen::RowMajor> *mat) {
//   mat->setZero();
//   mat->resize(n,m);
//   mat->setFromTriplets(coef.begin(), coef.end());
// }

// // replace the nonzero elements of mat to v
// template<class T>
// inline void ReplaceNZ(T v, Eigen::SparseMatrix<T,Eigen::RowMajor> *mat) {
//   T* values = mat->valuePtr();
//   for (int i = 0; i < mat->nonZeros(); ++i) {
//     values[i] = values[i] == 0 ? 0 : v;
//   }
// }

// static void LoadSpMatFromFile(string const&file,
//   ifstream in(file.c_str());
//   CHECK(in.good());
//   double row, col, nnz;
//   in >> row >> col >> nnz;
//   vector<Tri> coef;
//   for (int i = 0; i < (int)nnz; ++i) {
//     double r, c;
//     double v;
//     in >> r >> c >> v;
//     coef.push_back(Tri((int)r,(int)c,v));
//   }
//   in.close();
//   mat->resize((int)row, (int)col);
//   mat->setFromTriplets(coef.begin(), coef.end());
// }

// static void SaveSpMatToFile(string const& file, SpMat const& mat) {
//   ofstream out(file.c_str());
//   CHECK(out.good());
//   out << mat.rows() << "\t" << mat.cols() << "\t" << mat.nonZeros() << endl;
//   for (int i = 0; i < mat.rows(); ++i) {
//     for (int j = 0; j < mat.cols(); ++j) {
//       double v = mat.coeff(i,j);
//       if (v != 0) out << i << "\t" << j << "\t" << v << endl;
//     }
//   }
//   out.close();
// }

// static void SaveVecToFile(string const& file, Vec const& vec) {
//   ofstream out(file.c_str());
//   CHECK(out.good());
//   out << vec.size() << endl;
//   for (int i = 0; i < vec.size(); ++i) {
//     out << vec[i] << endl;
//   }
//   out.close();
// }

// static void LoadVecFromFile(string const&file, Vec *vec) {
//   ifstream in(file.c_str());
//   CHECK(in.good());
//   double dn;
//   in >> dn;
//   int n = (int) dn;
//   vec->resize(n,1);
//   for (int i = 0; i < n; ++i) {
//     in >> vec->coeffRef(i);
//   }
// }

static string DebugString(const dVec& vec) {
  string str = std::to_string(vec.size()) + ":[";
  int n = std::min((int)vec.size(), 2);
  for (int i = 0; i < n; ++i) {
    if (i < n - 1)
      str += std::to_string(vec[i]) + ", ";
    else
      str += std::to_string(vec[i]) + "]";
  }
  return str;
}

} // namespace PS

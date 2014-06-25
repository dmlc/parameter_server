#pragma once
#include "base/shared_array.h"
#include "base/dense_matrix.h"

namespace PS {

// zlib version
  // unsigned long res_size = size_;  // NOLINT
  // // Use uncompress() from zlib.h
  // const int ret =
  //     uncompress(reinterpret_cast<unsigned char*>(data_), &res_size,
  //                reinterpret_cast<const unsigned char*>(src), src_size);
  // CHECK_EQ(ret, Z_OK) << "Uncompress error occured! Error code: " << ret;
  // CHECK_LE(res_size, static_cast<unsigned long>(size_));  // NOLINT
  // resize(res_size);
template <typename V>
void SArray<V>::uncompressFrom(const char* src, size_t src_size) {
  size_t dsize = 0;
  CHECK(snappy::GetUncompressedLength(src, src_size, &dsize));
  resize(dsize);
  // CHECK_LE(dsize, size_);
  CHECK(snappy::RawUncompress(src, src_size, reinterpret_cast<char*>(data_)));
}

template <typename V>
bool SArray<V>::readFromFile(SizeR range, const string& file_name) {
  File* file = File::Open(file_name, "r");
  size_t size = file->Size();
  if (size % sizeof(V) != 0) {
    LL << file_name << " size: " << size
       << " cannot divided by sizeof(V): " << sizeof(V);
    return false;
  }
  if (range == SizeR::all()) range = SizeR(0, size/sizeof(V));
  CHECK(!range.empty());
  CHECK_LE(range.end()*sizeof(V), size);

  resize(range.size());
  file->seek(range.begin() * sizeof(V));
  size_t length = range.size() * sizeof(V);
  return (file->Read(ptr_.get(), length) == length);
}

template <typename V>
MatrixPtr<V> SArray<V>::matrix(size_t rows, size_t cols) {
  // TODO rows and cols
  MatrixInfo info;
  info.set_type(MatrixInfo::DENSE);
  info.set_row_major(false);
  SizeR(0, size_).to(info.mutable_row());
  SizeR(0,1).to(info.mutable_col());
  info.set_nnz(size_);
  info.set_sizeof_value(sizeof(V));
  return MatrixPtr<V>(new DenseMatrix<V>(info, *this));
}

template <typename V>
bool SArray<V>::writeToFile(SizeR range, const string& file_name) const {
  if (range == SizeR::all()) range = SizeR(0, size_);
  CHECK(!range.empty());
  CHECK_LE(range.end(), size_);

  File* file = File::Open(file_name, "w");
  size_t length = range.size() * sizeof(V);
  return (file->Write(ptr_.get(), length) == length);
}

} // namespace PS

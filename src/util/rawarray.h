#pragma once
#include <string>
#include "util/common.h"
#include "util/crc32c.h"

namespace PS {

// we use crc32 here
typedef uint32 cksum_t;

// an array without type, it servers as a vehicle to move data between classes
// without memcopying the content, that means, copying is by reference, NOT by
// value!

class RawArray {
 public:
  RawArray() { Fill(nullptr, 0, 0); }
  RawArray(size_t entry_size, size_t entry_num) {
    char* data = new char[entry_num*entry_size+5];
    Fill(data, entry_size, entry_num);
  }
  RawArray(char *data, size_t entry_size, size_t entry_num) {
    Fill(data, entry_size, entry_num);
  }
  // accessor
  char* data() { return data_.get(); }
  const char* data() const { return data_.get(); }
  size_t entry_size() const { return entry_size_; }
  size_t entry_num() const { return entry_num_; }
  size_t size() const { return entry_num_ * entry_size_; }
  cksum_t cksum() const { return cksum_; }
  bool empty() const { return entry_num_ == 0; }

  // fill the contents
  void Fill(char *data, size_t entry_size, size_t entry_num) {
    data_.reset(data);
    entry_size_ = entry_size;
    entry_num_ = entry_num;
    has_cksum_ = false;
    cksum_ = 0;
  }
  cksum_t ComputeCksum() {
    if (!has_cksum_) {
      has_cksum_ = true;
      if (!empty())
       cksum_ = crc32c::Value(data(), entry_num_*entry_size_);
    }
    return cksum_;
  }
  bool operator==(const RawArray &other) const {
    if (other.size() != size())
      return false;
    return memcmp(data(), other.data(), size()) == 0;
  }
  void ResetEntrySize(size_t new_entry_size) {
    size_t new_entry_num = size() / new_entry_size;
    CHECK_EQ(new_entry_num*new_entry_size, size());
    entry_num_ = new_entry_num;
    entry_size_ = new_entry_size;
  }
  bool Serialization(std::string *data) {
    std::string entry_size_str((char*)&entry_size_, sizeof(entry_size_));
    std::string entry_num_str((char*)&entry_num_, sizeof(entry_num_));
    std::string data_str(data_.get(), entry_size_ * entry_num_);
    *data = entry_size_str + entry_num_str + data_str;
    CHECK_EQ(data->length(), entry_size_ * entry_num_ + sizeof(entry_size_) + sizeof(entry_num_))
        << "data length: " << data->length() << " entry_size_: " << entry_size_ << " entry_num_: " << entry_num_;
    return true;
  }
  bool ParseFromString(const char *data) {
    const int64 *p = (const int64 *)data;
    entry_size_ = *p;
    ++p;
    entry_num_ = *p;
    ++p;
    int64 new_data_size = entry_size_ * entry_num_;
    char *new_data = new char[new_data_size];
    memcpy(new_data, p, new_data_size);
    data_.reset(new_data);
    return true;
  }
  void resetEntryNum(size_t newSize){
	  if(newSize==0){
		  clear();
		  return;
	  }
	  //TODO: test if this is safe. It should be.
	  char* t=data_.get();
	  CHECK_NOTNULL(t);
	  char* newData=(char*)realloc(t,newSize);
	  CHECK_NOTNULL(newData);
	  data_.reset(newData);
	  entry_num_=newSize;
  }
  void clear(){
	  entry_num_=0;
  }
 private:
  // the data stores the data
  shared_ptr<char> data_;
  // the size of each entry in byte
  int64 entry_size_;
  // the number of entries
  int64 entry_num_;
  // the check sum of the content in data_;
  bool has_cksum_;
  cksum_t cksum_;
};


} // namespace PS

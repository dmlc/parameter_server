#pragma once
#include "Eigen/Dense"
#include "parameter/shared_parameter.h"
#include "util/parallel_ordered_match.h"
#include "caffe/util/math_functions.hpp"
namespace PS {

template<typename V> class VVector;
template<typename V> using VVectorPtr = std::shared_ptr<VVector<V>>;


#define USING_SHARED_PARAMETER_INT8             \
  using Customer::port;                         \
  using Customer::myNodeID;                     \
  using SharedParameter<uint8>::get;                \
  using SharedParameter<uint8>::set;                \
  using SharedParameter<uint8>::myKeyRange;         \
  using SharedParameter<uint8>::keyRange;           \
  using SharedParameter<uint8>::sync

using caffe::caffe_copy;
using caffe::caffe_add;

// value vector only, access entirely ignoring keys/key ranges. key default to 0.
// values are stored in arrays.

template <typename V>
class VVListener{
public:
  virtual void vectorChanged(VVector<V>* data) = 0;
  virtual ~VVListener() {};
};

template <typename V>
class VVector : public SharedParameter<uint8> {
 public:
  VVector(const string& my_name, bool readonly = false, VVListener<V>* listener = nullptr, const string& parent_name = FLAGS_app_name, int k = 1) :
      SharedParameter<uint8>(my_name, parent_name), val_entry_size_(k) {
    this->listener = listener;
    this->readonly = readonly;
  }
  // VVector() : val_entry_size_(1) { }
  // VVector(int k) : val_entry_size_(k) { }
  virtual ~VVector() { }

  SArray<V>& value(int block_index = 0) { Lock l(mu_); return val_[block_index]; }
  void clear(int channel = 0) { Lock l(mu_); val_.erase(channel); }

  // find the local positions of a global key range
//  SizeR find(int channel, const Range<K>& key_range) {
//    return key(channel).findRange(key_range);
//  }

  int valueEntrySize() const { return val_entry_size_; }

  int vcount() {return this->val_.size();}

  int totalSize() {
    int sum = 0;
    for(int i = 0; i < val_.size(); i++){
	auto v = value(i);
	sum += v.size();
    }
    return sum;
  }
  // functions will used by the system
  MessagePtrList slice(const MessagePtr& msg, const KeyRangeList& sep);
  void getValue(const MessagePtr& msg);
  void setValue(const MessagePtr& msg);

  USING_SHARED_PARAMETER_INT8;
 protected:
  std::mutex mu_;
  std::unordered_map<int, SArray<V>> val_;
  int val_entry_size_;

  VVListener<V>* listener;
  bool readonly;
};

template <typename V>
void VVector<V>::setValue(const MessagePtr& msg) {
  CHECK(!readonly) << "VVector[" << name() << "] is read only!";
//  LL << "VVector::setValue received";
  // do check
  CHECK_EQ(msg->value.size(), val_.size()) << "my size(" << val_.size() << ") != message size(" << msg->value.size() << ")";
  for(int i = 0; i < msg->value.size(); i++){
      SArray<V> recv_val(msg->value[i]);
      auto& my_val = value(i);
      if (get(msg).has_tail_filter() || get(msg).gather()) {
        // join the received data with my current data
        SArray<V> new_val;
        if (recv_val.empty()) {
          CHECK(my_val.empty());
        } else {
          LL << "VVector::setValue before caffe_add";
          caffe::caffe_add(recv_val.size(), recv_val.data(), my_val.data(), my_val.data());
        }
      } else {
        CHECK_EQ(my_val.size(), recv_val.size()) << "VVector only support receiving whole VVector";
        my_val.copyFrom(recv_val);
      }
  }
  if(listener){
      listener->vectorChanged(this);
  }
//  LL << "VVector::setValue leaved";
}

template <typename V>
void VVector<V>::getValue(const MessagePtr& msg) {
//  LL << "VVector::getValue received";
  // get the data
  msg->clearValue();
  for(int i = 0; i < val_.size(); i++){
      msg->addValue(this->value(i));
  }
}

// partition is a sorted key ranges
template <typename V>
MessagePtrList VVector<V>::slice(const MessagePtr& msg, const KeyRangeList& sep) {
  if (get(msg).replica()) return Customer::slice(msg, sep);
  return sliceKeyOrderedMsg<uint8>(msg, sep);
}

} // namespace PS

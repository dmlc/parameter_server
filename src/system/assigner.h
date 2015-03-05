#pragma once
#include "util/common.h"
#include "system/proto/node.pb.h"
#include "data/proto/data.pb.h"
namespace PS {

// assign *node* with proper rank_id, key_range, etc..
class NodeAssigner {
 public:
  NodeAssigner() { }
  virtual ~NodeAssigner() { }

  virtual void assign(Node* node) {

  }

  virtual void remove(const Node& node) {
  }
 protected:
  int server_rank_ = 0;
  int worker_rank_ = 0;
};

// divide *data* into *num* parts.
class DataAssigner {
 public:
  DataAssigner() { }
  DataAssigner(const DataConfig& data, int num) { set(data, num); }
  ~DataAssigner() { }

  void set(const DataConfig& data, int num);
  bool next(DataConfig *data);

 private:
  std::vector<DataConfig> parts_;
  int cur_i = 0;
};

} // namespace PS

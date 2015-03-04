#include "util/common.h"
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

class TaskAssigner {

};

} // namespace PS

#include "system/shared_obj.h"

namespace PS {

class Box : public SharedObj {
 public:
  // global key range : [key_begin, key_end)
  // TODO allow each node has incomplete key_range, then the master do the union
  Box(const string& name, Key key_begin, Key key_end);
  // get the key range this node will maintain from the master
  virtual void Init();
  virtual void WaitInited();


 protected:
  bool box_inited_;
  KeyRange global_key_range_;
  // a segment of the global_key_range_ this node will maintain
  KeyRange local_key_range_;
};
} // namespace PS

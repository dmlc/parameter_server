#include "system/remote_node.h"
#include "system/customer.h"
#include "util/crc32c.h"
#include "util/shared_array_inl.h"
namespace PS {

FilterPtr RemoteNode::findFilter(const FilterConfig& conf) {
  // this lock is necessary, because encodeFileter and decodeFilter could be
  // called by different threads.
  Lock l(filter_mu_);
  int id = conf.type();
  auto it = filter_.find(id);
  if (it == filter_.end()) {
    filter_[id] = Filter::create(conf);
    it = filter_.find(id);
  }
  return it->second;
}

void RemoteNode::encodeFilter(const MessagePtr& msg) {
  const auto& tk = msg->task;
  for (int i = 0; i < tk.filter_size(); ++i) {
    findFilter(tk.filter(i))->encode(msg);
  }
}
void RemoteNode::decodeFilter(const MessagePtr& msg) {
  const auto& tk = msg->task;
  for (int i = tk.filter_size()-1; i >= 0; --i) {
    findFilter(tk.filter(i))->decode(msg);
  }
}


void RemoteNode::AddSubNode(RemoteNode* rnode) {
  CHECK_NOTNULL(rnode);
  // insert s into sub_nodes such as sub_nodes is still ordered
  int pos = 0;
  Range<Key> kr(rnode->rnode.key());
  while (pos < nodes.size()) {
    if (kr.inLeft(Range<Key>(CHECK_NOTNULL(nodes[pos])->key()))) {
      break;
    }
    ++ pos;
  }
  nodes.insert(nodes.begin() + pos, rnode);
  keys.insert(keys.begin() + pos, s->keyRange());
}

void RemoteNode::RemoveSubNode(RemoteNode* rnode) {
  size_t n = nodes.size();
  CHECK_EQ(n, keys.size());
  for (int i = 0; i < n; ++i) {
    if (sub_nodes[i] == s) {
      nodes.erase(nodes.begin() + i);
      keys.erase(keys.begin() + i);
      return;
    }
  }
}

} // namespace PS

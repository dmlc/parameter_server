#include "system/remote_node.h"
#include "system/customer.h"
#include "util/crc32c.h"
#include "util/shared_array_inl.h"
namespace PS {

Filter* RemoteNode::FindFilterOrCreate(const FilterConfig& conf) {
  int id = conf.type();
  auto it = filters.find(id);
  if (it == filters.end()) {
    filters[id] = Filter::create(conf);
    it = filters.find(id);
  }
  return it->second;
}

void RemoteNode::EncodeMessage(Message* msg) {
  const auto& tk = msg->task;
  for (int i = 0; i < tk.filter_size(); ++i) {
    FindFilterOrCreate(tk.filter(i))->encode(msg);
  }
}
void RemoteNode::DecodeMessage(Message* msg) {
  const auto& tk = msg->task;
  // a reverse order comparing to encode
  for (int i = tk.filter_size()-1; i >= 0; --i) {
    FindFilterOrCreate(tk.filter(i))->decode(msg);
  }
}

void RemoteNode::AddGroupNode(RemoteNode* rnode) {
  CHECK_NOTNULL(rnode);
  // insert s into sub_nodes such as sub_nodes is still ordered
  int pos = 0;
  Range<Key> kr(rnode->node.key());
  while (pos < group.size()) {
    if (kr.InLeft(Range<Key>(group[pos]->node.key()))) {
      break;
    }
    ++ pos;
  }
  group.insert(group.begin() + pos, rnode);
  keys.insert(keys.begin() + pos, kr);
}

void RemoteNode::RemoveGroupNode(RemoteNode* rnode) {
  size_t n = group.size();
  CHECK_EQ(n, keys.size());
  for (int i = 0; i < n; ++i) {
    if (group[i] == rnode) {
      group.erase(group.begin() + i);
      keys.erase(keys.begin() + i);
      return;
    }
  }
}

} // namespace PS

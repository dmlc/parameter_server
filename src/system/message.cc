#include "system/message.h"

namespace PS {

Message::Message(const NodeID& dest, int time, int wait_time)
    : recver(dest) {
  task.set_time(time);
  task.set_wait_time(wait_time);
}

std::string Message::shortDebugString() const {
  std::stringstream ss;
  if (task.request()) ss << "REQ"; else ss << "RLY";

  ss << " " << task.time() << " ";
  if (task.wait_time() >= 0) ss << "(wait " << task.wait_time() << ") ";
  ss << sender << "=>" << recver << " ";
  if (!original_recver.empty()) ss << "(" << original_recver << ") ";
  ss << "key [" << key.size() << "] value [";
  for (int i = 0; i < value.size(); ++i) {
    ss << value[i].size();
    if (i < value.size() - 1) ss << ",";
  }
  auto t = task; t.clear_msg();
  ss << "]. " << t.ShortDebugString();
  return ss.str();
}

std::string Message::debugString() const {
  std::stringstream ss;
  ss << "[message]: " << sender << "=>" << recver
     << "(" << original_recver << ")\n"
     << "[task]:" << task.ShortDebugString()
     << "\n[key]:" << key.size()
     << "\n[" << value.size() << " value]: ";
  for (const auto& x: value)
    ss << x.size() << " ";
  return ss.str();
}


} // namespace PS

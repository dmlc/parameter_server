#include "system/message.h"
namespace PS {

Message::Message(const NodeID& dest, int time, int wait_time)
    : recver(dest) {
  task.set_time(time);
  if (wait_time != kInvalidTime) task.add_wait_time(wait_time);
}

void Message::MiniCopyFrom(const Message& msg) {
  task = msg.task;
  // task.clear_value_type();
  task.clear_has_key();
  terminate = msg.terminate;
  callback = msg.callback;
}

FilterConfig* Message::add_filter(FilterConfig::Type type) {
  auto ptr = task.add_filter();
  ptr->set_type(type);
  return ptr;
}

size_t Message::mem_size() {
  size_t nbytes = task.SpaceUsed() + key.memSize();
  for (const auto& v : value) nbytes += v.memSize();
  return nbytes;
}

std::string Message::ShortDebugString() const {
  std::stringstream ss;
  if (key.size()) ss << "key [" << key.size() << "] ";
  if (value.size()) {
    ss << "value [";
    for (int i = 0; i < value.size(); ++i) {
      ss << value[i].size();
      if (i < value.size() - 1) ss << ",";
    }
    ss << "] ";
  }
  auto t = task; t.clear_msg(); ss << t.ShortDebugString();
  return ss.str();
}

std::string Message::DebugString() const {
  std::stringstream ss;
  ss << "[message]: " << sender << "=>" << recver
     << "[task]:" << task.ShortDebugString()
     << "\n[key]:" << key.size()
     << "\n[" << value.size() << " value]: ";
  for (const auto& x: value)
    ss << x.size() << " ";
  return ss.str();
}


} // namespace PS

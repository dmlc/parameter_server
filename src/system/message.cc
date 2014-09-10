#include "system/message.h"

namespace PS {

Message::Message(const NodeID& dest, int time, int wait_time)
    : recver(dest) {
  task.set_time(time);
  task.set_wait_time(wait_time);
}

// Message::Message(const Message& msg)
//     : task(msg.task), sender(msg.sender), recver(msg.recver),
//       original_recver(msg.original_recver), replied(msg.replied),
//       finished(msg.finished), valid(msg.valid), terminate(msg.terminate),
//       wait(msg.wait), recv_handle(msg.recv_handle), fin_handle(msg.fin_handle) { }

std::string Message::shortDebugString() const {
  std::stringstream ss;
  ss << "T: " << task.time() << ", " << sender << "=>" << recver;
  if (!original_recver.empty()) ss << "(" << original_recver << ")";
  ss << " wait_T: " << task.wait_time()
     << ", " << key.size() << " keys, " << value.size() << " value:";
  for (const auto& x: value)
    ss << " " << x.size();
  ss << ", task:" << task.ShortDebugString();
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

#include "system/message.h"

namespace PS {

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

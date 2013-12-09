#include "system/aggregator.h"

namespace PS {

// void Aggregator::Insert(const Mail& mail) {
//   if (!Valid()) return;
//   status_[mail.flag().time()][mail.flag().sender()] = mail;
// }

// bool Aggregator::Valid() const {
//   if (type_ == NodeGroup::kAll
//       || type_ == NodeGroup::kClients
//       || type_ == NodeGroup::kServers)
//     return true;
//   return false;
// }

// bool Aggregator::Success(time_t t, NodeGroup expect) const {
//   const auto it = status_.find(t);
//   if (it == status_.end())
//     return false;
//   NodeList nodes;
//   if (type_== NodeGroup::kAll)
//     nodes = expect.all();
//   else if (type_ == NodeGroup::kServers)
//     nodes = expect.servers();
//   else if (type_ == NodeGroup::kClients)
//     nodes = expect.clients();
//   const auto& st = it->second;
//   if (nodes->size() > st.size())
//     return false;
//   for (auto id : *nodes) {
//     if (st.find(id) == st.end())
//       return false;
//   }
//   return true;
// }

} // namespace PS

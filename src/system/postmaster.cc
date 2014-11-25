#include "system/postmaster.h"
#include "system/customer.h"
#include "data/common.h"
namespace PS {

std::vector<Node> Postmaster::nodes() {
  std::vector<Node> ret;
  for (const auto& it : obj_->sys_.yp().nodes()) {
    ret.push_back(it.second);
  }
  return ret;
}

std::vector<DataConfig>
Postmaster::partitionData(const DataConfig& conf, int num_workers) {
  auto data = searchFiles(conf);
  LI << "Found " << data.file_size() << " files";
  auto ret = divideFiles(data, num_workers);
  int n = 0; for (const auto& p : ret) n += p.file_size();
  LI << "Assigned " << n << " files to " << num_workers << " workers";
  return ret;
}

std::vector<Node>
Postmaster::partitionKey(const std::vector<Node>& nodes, Range<Key> range) {
  int num_servers = 0;
  auto ret = nodes;
  for (const auto& o : ret) {
    if (o.role() == Node::SERVER) ++ num_servers;
  }
  int s = 0;
  for (auto& o : ret) {
    auto key = o.role() != Node::SERVER ? range :
               range.evenDivide(num_servers, s++);
    key.to(o.mutable_key());
  }
  LI << "Paritition key range " << range << " into " << num_servers << " servers";
  return ret;
}

void Postmaster::createApp(
    const std::vector<Node>& nodes, const std::vector<AppConfig>& apps) {
  CHECK_EQ(obj_->myNodeID(), obj_->schedulerID());
  Task start;
  start.set_request(true);
  start.set_customer(obj_->name());
  start.set_type(Task::MANAGE);
  start.mutable_mng_node()->set_cmd(ManageNode::INIT);

  // add all node info
  CHECK_EQ(nodes.size(), apps.size());
  int n = nodes.size();
  for (int i = 0; i < n; ++i) {
    *start.mutable_mng_node()->add_node() = nodes[i];
  }

  // let the scheduler connect all other nodes
  auto& sys = Postoffice::instance();
  sys.manageNode(start);

  // create the app at all other machines
  std::vector<int> time(n);
  start.mutable_mng_app()->set_cmd(ManageApp::ADD);
  for (int i = 0; i < n; ++i) {
    if (nodes[i].id() == obj_->myNodeID()) continue;
    *start.mutable_mng_app()->mutable_app_config() = apps[i];
    auto o = obj_->taskpool(nodes[i].id());
    CHECK(o) << nodes[i].id();
    time[i] = o->submit(start);
  }

  // wait until finished
  for (int i = 0; i < n; ++i) {
    if (nodes[i].id() == obj_->myNodeID()) continue;
    obj_->taskpool(nodes[i].id())->waitOutgoingTask(time[i]);
  }
}


void Postmaster::stopApp() {
  // send terminate signal to all others
  Task terminate;
  terminate.set_type(Task::TERMINATE);
  auto pool = obj_->taskpool(kLiveGroup);
  if (!pool) {
    // so it's a single machine version. i need to send the terminal signal to
    // myself
    std::vector<Node> nodes(1, obj_->sys_.myNode());
    obj_->exec_.init(nodes);
    pool = obj_->taskpool(kLiveGroup);
  }
  pool->submit(terminate);
  // terminate.set_type(Task::TERMINATE_CONFIRM);
  usleep(800);
  LI << "System stopped\n";
}

} // namespace PS

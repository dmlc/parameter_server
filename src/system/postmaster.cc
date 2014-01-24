#include "system/postmaster.h"
#include "system/postoffice.h"
#include "system/shared_obj.h"
#include "util/status.h"

#include "box/container.h"
#include "app/inference.h"
namespace PS {

void Postmaster::Init() {
  postoffice_ = Postoffice::Instance();
  addr_book_.Init();
}

void Postmaster::Register(SharedObj *obj) {
  objects_[obj->id()] = obj;

}

KeyRange Postmaster::Register(Box *box, KeyRange global_key_range) {
  // a simple version, give box all available server nodes, and then evenly divide
  // the global_key_range
  NodeGroup group;
  if (addr_book_.IamRoot()) {

  }
}

// TODO ask master to get the range
void Postmaster::Register(Inference *ifr, DataRange whole) {
  // a simple version, give ifr all client nodes, and evenly divide the training samples
  // NodeGroup group;
  // size_t num = group_.clients()->size();
  // for (size_t i = 0; i < num; ++i) {
  //   uid_t id = group_.clients()->at(i);
  //   Workload wl(id, KeyRange(), whole.EvenDivide(num, i));
  //   workloads_[make_pair(ifr->name(), id)] = wl;
  //   group.clients()->push_back(id);
  //   group.all()->push_back(id);
  // }
  // nodegroups_[ifr->name()] = group;
}


  // // LL << "register is called";

  // if (my_uid_ == 0)
  // {
  //   dhtinfo_ = dht_->AssignNodes(ctr, whole, group_, all_);
  //   MasterAssignNodes(ctr, whole);
  // } else {
  //   SlaveAssignNodes(ctr, whole);
  // }

  // uid_t id = NodeGroup::kServers;
  // Workload wl(id, whole);
  // workloads_[make_pair(ctr->name(), id)] = wl;

  // return workloads_[make_pair(ctr->name(), my_uid())].key_range();


Express Postmaster::Reply(const Express& req) {
  Express ack;
  ack.set_command(req.command());
  ack.set_recver(req.sender());
  ack.set_seq_id(req.seq_id());
  ack.set_req(false);
  return ack;
}

void Postmaster::ProcessExpress(const Express& cmd) {
  if (cmd.req()) {

  }
  switch (cmd.command()) {
    case Express::ASSIGN_OBJ_ID: {
      if (cmd.req()) {
        CHECK(cmd.has_assign_id_req());
        Express reply = Reply(cmd);
        // TODO
        // reply.set_assign_id_ack(name_id_.GetID(cmd.assign_id_req()));
        postoffice_->Send(reply);
      } else {
        CHECK(cmd.has_assign_id_ack());
        postoffice_->SetExpressReply(cmd.seq_id(),
                                     to_string(cmd.assign_id_ack()));
      }
      break;
    }
    default:
      CHECK(false) << StrCat("unknow command: ", cmd.command());
  }
}

void Postmaster::NameToID(const string name, ExpressReply* fut) {
  Express cmd;
  cmd.set_recver(addr_book_.root().uid());
  cmd.set_command(Express::ASSIGN_OBJ_ID);
  cmd.set_assign_id_req(name);
  cmd.set_req(true);
  postoffice_->Send(cmd, fut);
}

const NodeGroup& Postmaster::GetNodeGroup(const string& name) const {
  const auto& it = nodegroups_.find(name);
  CHECK(it != nodegroups_.end());
  return it->second;
}

Workload* Postmaster::GetWorkload(const string& name, uid_t id) {
  const auto& it = workloads_.find(make_pair(name, id));
  CHECK(it != workloads_.end());
  return &it->second;
}

Container* Postmaster::GetContainer(const string& name) const {
  const auto& it = containers_.find(name);
  CHECK(it != containers_.end()) << "unknow container: " << name;
  return it->second;
}


}

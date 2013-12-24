#include "system/postmaster.h"
#include "system/postoffice.h"
#include "box/container.h"
#include "app/inference.h"
#include "util/status.h"
namespace PS {

void Postmaster::Init() {
  postoffice_ = Postoffice::Instance();
  addr_book_.Init();
  my_uid_ = addr_book_.my_uid();
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
}


Express Postmaster::Reply(const Express& req) {
  Express ack;
  ack.set_command(req.command());
  ack.set_recver(req.sender());
  ack.set_seq_id(req.seq_id());
  ack.set_req(false);
  return ack;
}

void Postmaster::ProcessExpress(const Express& cmd) {
  switch (cmd.command()) {
    case Express::ASSIGN_OBJ_ID: {
      LL << cmd.DebugString();
      if (cmd.req()) {
        CHECK(cmd.has_assign_id_req());
        Express reply = Reply(cmd);
        reply.set_assign_id_ack(name_id_.GetID(cmd.assign_id_req()));
        postoffice_->Send(reply, NULL);
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

void Postmaster::NameToID(const string name, CmdAck* fut) {
  Command cmd;
  cmd.set_recver(Root().uid());
  cmd.set_command_id(Command::ASSIGN_OBJ_ID);
  cmd.set_assign_id_req(name);
  sending_queue_.Put(make_pair(cmd, fut));
}


}

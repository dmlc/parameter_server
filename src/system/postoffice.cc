#include "system/postoffice.h"
#include "system/postmaster.h"
#include "box/container.h"
#include "system/replica_manager.h"

namespace PS {

DEFINE_bool(enable_fault_tolerance, false, "enable fault tolerance feature");

// remember to initialize ReplicaManager
void Postoffice::Init() {
  if (inited_) return;
  // initial connections
  postmaster_ = Postmaster::Instance();
  postmaster_->Init();
  // remember to initialize
  replica_manager_ = ReplicaManager::Instance();
  replica_manager_->Init();
  van_ = postmaster_->GetMailVan();
  // Van::Instance();
  // create two postman threads, one for sending, one for receiving
  send_postman_ = new std::thread(&Postoffice::SendPostman, this);
  recv_postman_ = new std::thread(&Postoffice::RecvPostman, this);
  inited_ = true;
}


// 1, fetch a mail, 2) divide the mail into several ones according to the
// destination machines. caches keys if necessary. 3) send one-by-one 4) notify
// the mail
void Postoffice::SendPostman() {
  while (1) {

    Mail mail = sending_queue_.Take();
    Header& head = mail.flag();
    const string& name = head.name();
    uid_t recver = head.recver();

    // check if is transfer packets
    if (head.type() == Header_Type_BACKUP) {
      LOG(WARNING) << "Header_Type_BACKUP send";
      head.set_sender(postmaster_->my_uid());
      CHECK(van_->Send(mail).ok());
      continue;
    }

    Workload *wl = postmaster_->GetWorkload(name, recver);
    CHECK(head.has_key());
    KeyRange kr(head.key().start(), head.key().end());
    // CHECK(kr.Valid()); // we may send invalid key range
    // first check whether the key list is cached
    bool hit = false;
    if (wl->GetCache(kr, mail.keys().ComputeCksum())) {
      hit = true;
    } else {
      wl->SetCache(kr, mail.keys().ComputeCksum(), mail.keys());
    }
    if (NodeGroup::Valid(recver)) {
      // the receiver is a group of nodes
      const NodeList& recvers = postmaster_->GetNodeGroup(name).Get(recver);
      // fetch the receiver list
      CHECK(!recvers->empty()) << "empty to " << recver;
      // divide the keys
      for (auto id : *recvers) {
        Workload *wl2 = postmaster_->GetWorkload(name, id);
        KeyRange kr2 = kr.Limit(wl2->key_range());
        RawArray key2, value2;
        bool hit2 = hit;
        // try to fetch the cached keys
        if (hit && !wl2->GetCache(kr2, 0, &key2)) {
          hit2 = false;
        }
        // failed to fetch the keys
        if (!hit2) {
          // slice the according keys and then store in cache
          key2 = Slice(mail.keys(), kr2);
          cksum_t cksum = key2.ComputeCksum();
          wl2->SetCache(kr2, cksum, key2);
        }
        if (head.has_value() && !head.value().empty()) {
          value2 = Slice(mail.keys(), mail.vals(), key2);
        }
        head.set_recver(id);
        head.set_sender(postmaster_->my_uid());
        head.mutable_key()->set_start(kr2.start());
        head.mutable_key()->set_end(kr2.end());
        head.mutable_key()->set_cksum(key2.cksum());
        head.mutable_key()->set_empty(hit2);
        Mail mail2(head, key2, value2);
        CHECK(van_->Send(mail2).ok());
      }
    } else {
      // the receiver is a single node
      head.set_sender(postmaster_->my_uid());
      head.mutable_key()->set_empty(hit);
      head.mutable_key()->set_cksum(mail.keys().cksum());
      CHECK(van_->Send(mail).ok());
    }
    postmaster_->GetContainer(name)->Notify(mail.flag());
  }
}

// if mail does not have key, fetch the cached keys. otherwise, cache the keys
// in mail.
void Postoffice::RecvPostman() {
  Mail mail;
  while(1) {

    // distinguish node types
    // normal mail send it to container
    // back up key-value mail send it to replica nodes
    // put it in the replica manager queue
    // replica key-value mail send it to replica manager
    // node management info send it to postmaster queue
    // rescue mail, send it to the replica manager
    van_->Recv(&mail);
    LOG(WARNING) << "receive a packet";
    const Header& head = mail.flag();
    // check if is a backup mail or a rescue mail
    if (FLAGS_enable_fault_tolerance) {
      if (head.type() == Header_Type_BACKUP || head.type() == Header_Type_NODE_RESCUE) {
        replica_manager_->Put(mail);
        continue;
      }
    }

    // deal with key caches
    KeyRange kr(head.key().start(), head.key().end());
    CHECK(kr.Valid());
    Workload *wl = postmaster_->GetWorkload(head.name(), head.sender());
    CHECK(head.has_key());
    auto cksum = head.key().cksum();
    if (!head.key().empty()) {
      CHECK_EQ(cksum, mail.keys().ComputeCksum());
      mail.keys().ResetEntrySize(sizeof(Key));
      if (!wl->GetCache(kr, cksum, NULL))
      {
        LL <<  "I caching "<< head.sender() <<" "<< head.key().start() << head.key().end();
        wl->SetCache(kr, cksum, mail.keys());
      }
    } else {
      RawArray keys;
      LL <<"I am" << postmaster_->my_uid() << " looking for cach " << head.sender() << " " << head.key().start() << " " <<head.key().end();
      // TODO a fault tolerance way is just as the sender to resend the keys
      CHECK(wl->GetCache(kr, cksum, &keys))
          << "keys" << kr.ToString() << " of " << head.name() << " are not cached";
      mail.set_keys(keys);
    }
    if (!mail.keys().empty()) {
      mail.vals().ResetEntrySize(mail.vals().size() / mail.keys().entry_num());
    }

    //
    postmaster_->GetContainer(head.name())->Accept(mail);
    
    LOG(WARNING) << "before FLAGS_enable_fault_tolerance";
    if (FLAGS_enable_fault_tolerance && !postmaster_->IamClient()) {
      LOG(WARNING) << "in FLAGS_enable_fault_tolerance";
      replica_manager_->Put(mail);
    }
  }
}


} // namespace PS

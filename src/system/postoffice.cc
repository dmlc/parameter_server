#include "system/postoffice.h"
// #include <omp.h>
#include "system/customer.h"
#include "app/app.h"
// #include "system/postmaster.h"
// #include "system/node_group.h"
#include "system/debug.h"
#include "util/file.h"

namespace PS {

DEFINE_bool(enable_fault_tolerance, false, "enable fault tolerance feature");

DEFINE_int32(num_servers, 1, "number of servers");
DEFINE_int32(num_workers, 1, "number of clients");
DEFINE_int32(num_unused, 0, "number of unused nodes");

DEFINE_int32(num_replicas, 0, "number of replica");

DEFINE_string(app, "../config/rcv1_l1lr.config", "the configuration file of app");
// DEFINE_string(app, "../config/block_prox_grad.config", "the configuration file of app");

DEFINE_string(node_file, "./nodes", "node information");

DEFINE_int32(num_threads, 2, "number of computational threads");

Postoffice::~Postoffice() {
  // sending_->join();
  // yp_.van().destroy();
  recving_->join();
  Message stop; stop.terminate = true; queue(stop);
  sending_->join();
}

void Postoffice::run() {
  yp_.init();
  recving_ = std::unique_ptr<std::thread>(new std::thread(&Postoffice::recv, this));
  sending_ = std::unique_ptr<std::thread>(new std::thread(&Postoffice::send, this));

  // omp_set_dynamic(0);
  // omp_set_num_threads(FLAGS_num_threads);
  if (myNode().role() == Node::SCHEDULER) {
    // run the application
    AppConfig config;
    ReadFileToProtoOrDie(FLAGS_app, &config);
    AppPtr app = App::create(config);
    yp_.add(app);
    app->run();
    app->stop();
  } else {
    // run as a daemon
    while (!done_) usleep(300);
    // LL << myNode().uid() << " stopped";
    // std::this_thread::yield();
    // usleep(2000);
  }
}

void Postoffice::reply(const Message& msg, const string& reply_msg) {
  if (!msg.task.request()) return;
  Message re = replyTemplate(msg);
  re.task.set_type(Task::REPLY);
  if (!reply_msg.empty())
    re.task.set_msg(reply_msg);
  re.task.set_time(msg.task.time());
  queue(re);
}

void Postoffice::queue(const Message& msg) {

  // if (msg.task.time() == 1450) LL << msg.shortDebugString();
  if (msg.valid) {
    sending_queue_.push(msg);
    // if (msg.sender == "S29") { //} && msg.recver == "U0") {
    // // LL << msg.shortDebugString()<<"\n";
    //   if (sending_queue_.size() < 200)
    //     LL << sending_queue_.size() << " " << msg;
    // }
  } else {
    // do not send, fake a reply mesage
    // LL << myNode().id() << " " << msg.shortDebugString();
    Message re = replyTemplate(msg);
    re.task.set_type(Task::REPLY);
    re.task.set_time(msg.task.time());
    yp_.customer(re.task.customer())->exec().accept(re);
  }
}

//  TODO fault tolerance, check if node info has been changed
void Postoffice::send() {
  Message msg;
  // send out all messge in the queue even done
  while (true) {
    sending_queue_.wait_and_pop(msg);
    if (msg.terminate) break;
    // if (msg.sender == "S29" ) LL << sending_queue_.size() << " " << msg;
    // LL <<  msg.shortDebugString()<<"\n";
    // CHECK(stat.ok()) << "error: " << stat.ToString();

    Status stat = yp_.van().send(msg);
    if (!stat.ok()) {
      LL << "sending " << msg.debugString() << " failed\n"
         << "error: " << stat.ToString();
    }
  }
}

void Postoffice::recv() {
  Message msg;
  // bool shutting_down = false;
  while (true) {
    auto stat = yp_.van().recv(&msg);
    // if (!stat.ok()) break;
    CHECK(stat.ok()) << stat.ToString();
    auto& tk = msg.task;
    // check if I could do something
    // if (shutting_down) {
    //   if (tk.request() && tk.type() == Task::TERMINATE_CONFIRM) {
    //     done_ = true;
    //     break;
    //   } else {
    //     continue;
    //   }
    // }
    if (tk.request() && tk.type() == Task::TERMINATE) {
      yp_.van().statistic();
      // reply(msg);
      // shutting_down = true;
      done_ = true;
      break;
    } else if (tk.request() && tk.type() == Task::MANAGE) {
      if (tk.has_mng_app()) manage_app(tk);
      if (tk.has_mng_node()) manage_node(tk);
    } else {
      yp_.customer(tk.customer())->exec().accept(msg);
      continue;
    }
    auto ptr = yp_.customer(tk.customer());
    if (ptr != nullptr) ptr->exec().finish(msg);
    reply(msg);
  }
}

void Postoffice::manage_app(const Task& tk) {
  CHECK(tk.has_mng_app());
  auto& mng = tk.mng_app();
  if (mng.cmd() == ManageApp::ADD) {
    yp_.add(std::static_pointer_cast<Customer>(App::create(mng.app_config())));
  }
}

void Postoffice::manage_node(const Task& tk) {
  // LL << tk.DebugString();
  CHECK(tk.has_mng_node());
  auto& mng = tk.mng_node();
  std::vector<Node> nodes;
  for (int i = 0; i < mng.nodes_size(); ++i)
    nodes.push_back(mng.nodes(i));

  auto obj = yp_.customer(tk.customer());
  switch (mng.cmd()) {
    case ManageNode::INIT:
      for (auto n : nodes) yp_.add(n);
      if (obj != nullptr) {
        obj->exec().init(nodes);
        for (auto c : obj->child_customers())
          yp_.customer(c)->exec().init(nodes);
      }
      break;

    case ManageNode::REPLACE:
      CHECK_EQ(nodes.size(), 2);
      obj->exec().replace(nodes[0], nodes[1]);
      for (auto c : obj->child_customers())
        yp_.customer(c)->exec().replace(nodes[0], nodes[1]);
      break;

    default:
      CHECK(false) << " unknow command " << mng.cmd();
  }

}

// Ack Postoffice::send(Mail pkg) {
//   CHECK(pkg.label().has_request());
//   if (!pkg.label().request()) {
//     sending_queue_.push(std::move(pkg));
//     return Ack();
//   }
//   std::promise<std::string> pro;
//   {
//     Lock l(mutex_);
//     pkg.label().set_tracking_num(tracking_num_);
//     promises_[tracking_num_++] = std::move(pro);
//   }
//   sending_queue_.push(std::move(pkg));
//   return pro.get_future();
// }

// void Postoffice::sendThread() {

//   Mail pkg;
//   while (!done_) {
//     if (sending_queue_.try_pop(pkg)) {
//      auto& label = pkg.label();
//     // int cust_id = label.customer_id();
//     int recver = label.recver();
//     label.set_sender(yp_.myNode().uid());
//     if (!NodeGroup::Valid(recver)) {
//       // the receiver is a single node
//       Status stat = yp_.van().Send(pkg);
//       // TODO fault tolerance
//       CHECK(stat.ok()) << stat.ToString();
//     }
//     } else {
//       std::this_thread::yield();
//     }

//     //     yellow_pages_.GetCustomer(cust_id)->Notify(pkg.label());
//   }
// }


// void Postoffice::SendExpress() {
//   while(true) {
//     Express cmd = express_sending_queue_.Take();
//     cmd.set_sender(postman_.my_uid());
//     Status stat = postman_.express_van()->Send(cmd);
//     CHECK(stat.ok()) << stat.ToString();
//   }
// }

// void Postoffice::RecvExpress() {
//   Express cmd;
//   while(true) {
//     Status stat = postman_.express_van()->Recv(&cmd);
//     postmaster_.ProcessExpress(cmd);
//   }
// }

//     // check if is transfer packets
//     // if (head.type() == Header_Type_BACKUP) {
//     //   // LOG(WARNING) << "Header_Type_BACKUP send";
//     //   head.set_sender(postmaster_->my_uid());
//     //   CHECK(package_van_->Send(mail).ok());
//     //   continue;
//     // }
// // 1, fetch a mail, 2) divide the mail into several ones according to the
// // destination machines. caches keys if necessary. 3) send one-by-one 4) notify
// // the mail
// void Postoffice::SendPackage() {
//   while (1) {
//     Package pkg = package_sending_queue_.Take();
//     auto& label = pkg.label();
//     int32 cust_id = label.customer_id();
//     int32 recver = label.recver();
//     Workload *wl = yellow_pages_.GetWorkload(cust_id, recver);
//     CHECK(label.has_key());
//     KeyRange kr(label.key().start(), label.key().end());
//     // CHECK(kr.Valid()); // we may send invalid key range
//     // first check whether the key list is cached
//     bool hit = false;
//     if (FLAGS_enable_key_cache) {
//       if (wl->GetCache(kr, pkg.keys().ComputeCksum()))
//         hit = true;
//       else
//         wl->SetCache(kr, pkg.keys().cksum(), pkg.keys());
//     }
//     // now send the package
//     if (!NodeGroup::Valid(recver)) {
//       // the receiver is a single node
//       label.set_sender(postman_.my_uid());
//       label.mutable_key()->set_empty(hit);
//       label.mutable_key()->set_cksum(pkg.keys().cksum());
//       Status stat = postman_.package_van()->Send(pkg);
//       // TODO fault tolerance
//       CHECK(stat.ok()) << stat.ToString();
//     } else {
//       // the receiver is a group of nodes, fetch the node list
//       const NodeList& recvers = yellow_pages_.GetNodeGroup(cust_id).Get(recver);
//       CHECK(!recvers->empty()) << "no nodes associated with " << recver;
//       // divide the keys according to the key range a node maintaining
//       for (auto node_id : *recvers) {
//         Workload *wl2 = yellow_pages_.GetWorkload(cust_id, node_id);
//         KeyRange kr2 = kr.Limit(wl2->key_range());
//         RawArray key2, value2;
//         bool hit2 = hit;
//         // try to fetch the cached keys, we do not compute the checksum here to
//         // save the computational time. but it may be not safe.
//         if (hit && !wl2->GetCache(kr2, 0, &key2)) {
//           hit2 = false;
//         }
//         if (!hit2) {
//           // slice the according keys and then store in cache
//           key2 = Slice(pkg.keys(), kr2);
//           if (FLAGS_enable_key_cache)
//             wl2->SetCache(kr2, key2.ComputeCksum(), key2);
//         }
//         if (label.has_value() && !label.value().empty())
//           value2 = Slice(pkg.keys(), pkg.vals(), key2);
//         label.set_recver(node_id);
//         label.set_sender(postman_.my_uid());
//         label.mutable_key()->set_start(kr2.start());
//         label.mutable_key()->set_end(kr2.end());
//         label.mutable_key()->set_cksum(key2.cksum());
//         label.mutable_key()->set_empty(hit2);
//         Package pkg2(label, key2, value2);
//         Status stat = postman_.package_van()->Send(pkg2);
//         // TODO fault tolerance
//         CHECK(stat.ok()) << stat.ToString();
//       }
//     }
//     // notify the customer that package has been sent
//     yellow_pages_.GetCustomer(cust_id)->Notify(pkg.label());
//   }
// }

//     // distinguish node types
//     // normal mail send it to container
//     // back up key-value mail send it to replica nodes
//     // put it in the replica manager queue
//     // replica key-value mail send it to replica manager
//     // node management info send it to postmaster queue
//     // rescue mail, send it to the replica manager
//     // check if is a backup mail or a rescue mail
//     // if (FLAGS_enable_fault_tolerance) {
//     //   if (head.type() == Header_Type_BACKUP
//     //       || head.type() == Header_Type_NODE_RESCUE) {
//     //     replica_manager_->Put(mail);
//     //     continue;
//     //   }
//     // }

//     // if (FLAGS_enable_fault_tolerance && !postmaster_->IamClient()) {
//     //   replica_manager_->Put(pkg);
//     // }
// // if mail does not have key, fetch the cached keys. otherwise, cache the keys
// // in mail.
// void Postoffice::RecvPackage() {
//   Package pkg;
//   while(1) {
//     Status stat = postman_.package_van()->Recv(&pkg);
//     // TODO fault tolerance
     // CHECK(stat.ok()) << stat.ToString();
//     const auto& label = pkg.label();
//     int32 cust_id = label.customer_id();
//     auto cust = yellow_pages_.GetCustomer(cust_id);
//     // waiting is necessary
//     cust->WaitInited();
//     // deal with key caches
//     CHECK(label.has_key());
//     KeyRange kr(label.key().start(), label.key().end());
//     CHECK(kr.Valid());
//     Workload *wl = yellow_pages_.GetWorkload(cust_id, label.sender());
//     auto cksum = label.key().cksum();
//     if (!label.key().empty()) {
//       // there are key lists
//       CHECK_EQ(cksum, pkg.keys().ComputeCksum());
//       // ensure the keys() has proper length when giving to the customer
//       pkg.keys().ResetEntrySize(sizeof(Key));
//       if (FLAGS_enable_key_cache && !wl->GetCache(kr, cksum, NULL))
//         wl->SetCache(kr, cksum, pkg.keys());
//     } else {
//       // the sender believe I have cached the key lists. If it is not true,
//       // TODO a fault tolerance way is just as the sender to resend the keys
//       RawArray keys;
//       CHECK(wl->GetCache(kr, cksum, &keys))
//           << "keys" << kr.ToString() << " of " << cust_id << " are not cached";
//       pkg.set_keys(keys);
//     }
//     if (!pkg.keys().empty()) {
//       pkg.vals().ResetEntrySize(pkg.vals().size() / pkg.keys().entry_num());
//     }
//     cust->Accept(pkg);
//   }
// }

} // namespace PS

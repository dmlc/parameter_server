#include "system/postoffice.h"
#include "system/customer.h"
#include "util/file.h"
// #include <omp.h>

namespace PS {

DEFINE_int32(report_interval, 0,
  "Servers/Workers report running status to the scheduler "
  "in every report_interval seconds. "
  "default: 0; if set to 0, heartbeat is disabled");

DECLARE_string(interface);

Postoffice::Postoffice() { }

Postoffice::~Postoffice() {
  if (recv_thread_) recv_thread_->join();
  if (send_thread_) {
    Message* stop = new Message(); stop->terminate = true; Queue(stop);
    send_thread_->join();
  }
}

void Postoffice::Run(int* argc, char*** argv) {
  google::InitGoogleLogging((*argv)[0]);
  google::ParseCommandLineFlags(argc, argv, true);

  manager_.Init((*argv)[0]);

  if (FLAGS_report_interval > 0) {
    perf_monitor_.init(FLAGS_interface, manager_.van().my_node().hostname());
  }

  // start the I/O threads
  recv_thread_ =
      std::unique_ptr<std::thread>(new std::thread(&Postoffice::Recv, this));
  send_thread_ =
      std::unique_ptr<std::thread>(new std::thread(&Postoffice::Send, this));

  manager_.Run();
}

void Postoffice::Send() {
  Message* msg;
  while (true) {
    sending_queue_.wait_and_pop(msg);
    if (msg->terminate) break;
    size_t send_bytes = 0;
    manager_.van().Send(msg, &send_bytes);
    if (FLAGS_report_interval > 0) {
      perf_monitor_.increaseOutBytes(send_bytes);
    }
    if (msg->task.request()) {
      // a request "msg" is safe to be deleted only if the response is received
      manager_.AddRequest(msg);
    } else {
      delete msg;
    }
  }
}

void Postoffice::Queue(Message* msg) {
  if (!msg->task.has_more()) {
    sending_queue_.push(msg);
  } else {
    // do pack
    CHECK(msg->task.request());
    CHECK(msg->task.has_customer_id());
    CHECK(!msg->has_data()) << " don't know how to pack data";
    Lock lk(pack_mu_);
    auto key = std::make_pair(msg->recver, msg->task.customer_id());
    auto& value = pack_[key];
    value.push_back(msg);

    if (!msg->task.more()) {
      // it's the final message, pack and send
      Message* pack_msg = new Message();
      pack_msg->recver = msg->recver;
      for (auto m : value) {
        m->task.clear_more();
        *pack_msg->task.add_task() = m->task;
        delete m;
      }
      value.clear();
      sending_queue_.push(pack_msg);
    }
  }
}

void Postoffice::Recv() {
  while (true) {
    // receive a message
    Message* msg = new Message();
    size_t recv_bytes = 0;
    CHECK(manager_.van().Recv(msg, &recv_bytes));
    if (FLAGS_report_interval > 0) {
      perf_monitor_.increaseInBytes(recv_bytes);
    }

    if (msg->task.task_size()) {
      // packed task
      CHECK(!msg->has_data());
      for (int i = 0; i < msg->task.task_size(); ++i) {
        Message* unpack_msg = new Message();
        unpack_msg->recver = msg->recver;
        unpack_msg->sender = msg->sender;
        unpack_msg->task = msg->task.task(i);
        if (!Process(unpack_msg)) break;
      }
      delete msg;
    } else {
      if (!Process(msg)) break;
    }
  }
}

<<<<<<< HEAD
void Postoffice::manageNode(Task& tk) {
  CHECK(tk.has_mng_node());
  auto& mng = tk.mng_node();
  switch (mng.cmd()) {
    case ManageNode::CONNECT: {
      CHECK(IamScheduler());
      CHECK_EQ(mng.node_size(), 1);
      // first add this node into app
      Task add = tk;
      add.set_customer(CHECK_NOTNULL(app_)->name());
      add.mutable_mng_node()->set_cmd(ManageNode::ADD);
      manageNode(add);
      // create the app in this node
      Task task;
      task.set_request(true);
      task.set_customer(app_->name());
      task.set_type(Task::MANAGE);
      task.set_time(1);
      task.mutable_mng_app()->set_cmd(ManageApp::ADD);
      task.mutable_mng_app()->set_conf(app_conf_);
      app_->port(mng.node(0).id())->submit(task);
      // check if all nodes are connected
      if (yp().num_workers() >= FLAGS_num_workers &&
          yp().num_servers() >= FLAGS_num_servers) {
        nodes_are_ready_.set_value();
      }
      tk.set_customer(app_->name());  // otherwise the remote node doesn't know
      // how to find the according customer
      break;
    }
    case ManageNode::ADD:
    case ManageNode::UPDATE: {
      auto obj = yp().customer(tk.customer());
      CHECK(obj) << "customer [" << tk.customer() << "] doesn't exists";
      for (int i = 0; i < mng.node_size(); ++i) {
        auto node = mng.node(i);
        yp().addNode(node);
        obj->exec().add(node);
        for (auto c : yp().children(obj->name())) {
          auto child = yp().customer(c);
          if (child) child->exec().add(node);
        }
      }
      break;
    }
    case ManageNode::REPLACE: {
      break;
    }
    case ManageNode::REMOVE: {
      break;
    }
=======
bool Postoffice::Process(Message* msg) {
  if (!msg->task.request()) manager_.AddResponse(msg);
  // process this message
  if (msg->task.control()) {
    bool ret = manager_.Process(msg);
    delete msg;
    return ret;
  } else {
    int id = msg->task.customer_id();
    // let the executor to delete "msg"
    manager_.customer(id)->executor()->Accept(msg);
>>>>>>> upstream/master
  }
  return true;
}


} // namespace PS

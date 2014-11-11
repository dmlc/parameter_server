#pragma once
#include "system/app.h"

namespace PS {
class ParsaWorker;
class ParsaServer;
class Parsa : public App {
 public:
  Parsa() { }
  ~Parsa();
  void init();
  void run();
  void process(const MessagePtr& msg) { }
 private:
  int num_partitions_;
  ParsaConf conf_;
  ParsaWorker* worker_ = nullptr;
  ParsaServer* server_ = nullptr;
};

} // namespace PS

// I didn't use unique_ptr here, because i don't want to include "parsa_worker.h"
// std::unique_ptr<ParsaWorker> worker_;
// std::unique_ptr<ParsaServer> server_;

#pragma once
#include "system/app.h"

namespace PS {
class ParsaWorker;
class ParsaServer;
class Parsa : public App {
 public:
  void init();
  void run();
  void process(const MessagePtr& msg) { }
 private:
  int num_partitions_;
  ParsaConf conf_;
  ParsaWorker* worker_;
  // std::unique_ptr<ParsaWorker> worker_;
  // std::unique_ptr<ParsaServer> server_;
};

} // namespace PS

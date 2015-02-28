#include "ps.h"
#include "updater.h"
#include "shared_model.h"
#include "minerva_ps.h"
#include "minerva_server.h"

PS::App* PS::CreateServerNode(const std::string& conf) {
  return new minerva::MinervaServer();
}

int WorkerNodeMain(int argc, char *argv[]) {
  return ::MinervaWorkerMain(PS::MyRank(), PS::RankSize(), argc, argv);
}
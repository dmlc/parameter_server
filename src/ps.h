#pragma once
#include "system/postoffice.h"
#include "system/app.h"

int PSMain(int argc, char *argv[]);
PS::App* CreateServer(const std::string& conf);

inline int PSRank() {
  return PS::Postoffice::instance().myNode().rank();
}

inline std::string PSNodeID() {
  return PS::Postoffice::instance().myNode().id();
}

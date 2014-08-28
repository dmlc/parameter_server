#include "system/postoffice.h"

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  PS::Postoffice::instance().run();

  return 0;
}

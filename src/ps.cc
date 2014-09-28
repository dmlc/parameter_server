#include "system/postoffice.h"

namespace PS {

} // namespace
int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  PS::Postoffice::instance().run();

  return 0;
}

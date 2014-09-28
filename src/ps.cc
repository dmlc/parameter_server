#include "system/postoffice.h"

namespace PS {
int64 g_mem_usage_sarray = 0;
std::mutex g_mu_sa_;

} // namespace
int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  PS::Postoffice::instance().run();

  return 0;
}

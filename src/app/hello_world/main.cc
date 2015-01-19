#include "system/app.h"

namespace PS {

class Hello : public App {
  void init() { LL << "..."; }
  void run() { LL << "run"; }
};

App* App::create(const string& name, const string& conf) {
  return new Hello();
}

} // namespace PS

int main(int argc, char *argv[]) {
  PS::Postoffice::instance().start(argc, argv);

  PS::Postoffice::instance().stop();
  return 0;
}

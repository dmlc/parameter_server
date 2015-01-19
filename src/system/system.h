#include "system/app.h"
namespace PS {

class Engine : public Customer {
 public:
  Engine() : Customer("engine") { }
  virtual ~Engine() { }

  void start() {

  }
  void stop() {

  }

 private:
};

} // namespace PS

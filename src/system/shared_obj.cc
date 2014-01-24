#include "system/shared_obj.h"

namespace PS {

void SharedObj::Init() {
  // init office and master, if they are not inited yet
  postoffice_ = Postoffice::Instance();
  postoffice_->Init();
  postmaster_ = Postmaster::Instance();

  // get assigned id
  ExpressReply fut;// = new ExpressReply;
  postmaster_->NameToID(name(), &fut);
  try {
    id_ = std::stoi(fut.get());
  } catch (const std::future_error& e) {
    CHECK(false) << "Caught a future_error with code \"" << e.code()
                 << "\"\nMessage: \"" << e.what() << "\"\n";
  }
  obj_inited_ = true;
}

void SharedObj::WaitInited() {
  while (!obj_inited_)
    std::this_thread::sleep_for(milliseconds(100));
}

} // namespace PS

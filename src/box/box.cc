#include "box/box.h"

namespace PS {

Box::Box(const string& name, Key key_begin, Key key_end) :
    : SharedObj(name), box_inited_(false) {
  global_key_range_.Set(key_begin, key_end);
}

void Box::Init() {
  SharedObj::Init();
  local_key_range_ = postmaster_->Register(this, global_key_range_);
  box_inited_ = true;
  LL << SName() << " my key range " << local_key_range_.ToString();
}

void Box::WaitInited() {
  SharedObj::WaitInited();
  while (!box_inited_) {
    LL << "waiting container(" << name() << ") is initialized";
    std::this_thread::sleep_for(milliseconds(100));
  }
}

} // namespace PS

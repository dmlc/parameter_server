#include "system/shared_obj.h"

namespace PS {

void SharedObj::Init() {
  // init office and master, if they are not inited yet
  postoffice_ = Postoffice::Instance();
  postoffice_->Init();
  postmaster_ = Postmaster::Instance();

  // get assigned id
  ExpressReply fut;
  postmaster_->NameToID(name(), &fut);
  id_ = std::stoi(fut.get());
  obj_inited_ = true;
}

} // namespace PS

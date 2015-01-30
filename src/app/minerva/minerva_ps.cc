#include "minerva_ps.h"
#include "shared_model.h"


static PS::SharedModel<float> *shared_model = nullptr;

namespace minerva {
namespace basic {

// shared_model = nullptr;

void PushGradAndPullWeight(const float * grad, float * weight, size_t size,
                           const std::string & layer_name) {
  if (!shared_model) {
    shared_model = new PS::SharedModel<float>();
  }

  // push
  using namespace PS;
  int push_time = -1;
  if (grad) {
    SArray<float> val; val.copyFrom(grad, size);
    MessagePtr push_msg(new Message(kServerGroup));
    push_msg->addValue(val);
    // LL << val;
    push_msg->task.set_key_channel_str(layer_name);
    Range<Key>(0, size).to(push_msg->task.mutable_key_range());
    push_time = CHECK_NOTNULL(shared_model)->push(push_msg);
  }

  // pull
  shared_model->setLayer(layer_name, weight, size);
  MessagePtr pull_msg(new Message(kServerGroup, -1, push_time));
  pull_msg->task.set_key_channel_str(layer_name);
  Range<Key>(0, size).to(pull_msg->task.mutable_key_range());
  pull_msg->wait = true;
  shared_model->pull(pull_msg);
}

}
}

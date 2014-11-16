#include "linear_method/computation_node.h"
namespace PS {
namespace LM {

void CompNode::process(const MessagePtr& msg) {
  switch (get(msg).cmd()) {
    case Call::EVALUATE_PROGRESS: {
      Progress prog; evaluateProgress(&prog);
      sys_.replyProtocalMessage(msg, prog);
      break;
    }
    case Call::LOAD_DATA: {
      DataInfo info;
      int hit_cache = 0;
      loadData(info.mutable_example_info(), &hit_cache);
      info.set_hit_cache(hit_cache);
      sys_.replyProtocalMessage(msg, info);
      break;
    }
    case Call::PREPROCESS_DATA:
      preprocessData(msg);
      break;
    case Call::UPDATE_MODEL:
      iterate(msg);
      break;
    case Call::SAVE_MODEL:
      saveModel();
      break;
    case Call::RECOVER:
      // FIXME
      // W_.recover();
      break;
    case Call::COMPUTE_VALIDATION_AUC: {
      // FIXME
      // AUCData data;
      // computeEvaluationAUC(&data);
      // sys_.replyProtocalMessage(msg, data);
      break;
    }
    default:
      CHECK(false) << "unknown cmd: " << get(msg).cmd();
  }
}

} // namespace LM
} // namespace PS

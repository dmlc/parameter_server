#include "util/common.h"
#include "proto/instance.pb.h"
#include "proto/config.pb.h"

namespace PS {

class ParseText {
 public:
  typedef DataConfig::TextFormat TextFormat;
  // using DataConfig::TextFormat;
  void setFormat(TextFormat format);
  bool toProto(char* line, Instance* ins);
  InstanceInfo info();
 private:
  bool parseLibsvm(char*, Instance*);
  bool parseAdfea(char*, Instance*);

  typedef std::function<bool(char*, Instance*)> Convertor;
  Convertor convertor_;
  InstanceInfo info_;
  std::map<int, FeatureGroupInfo> group_info_;
};

}

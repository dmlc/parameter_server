// deprecated, see example_parser.h

#pragma once
#include "util/common.h"
#include "proto/instance.pb.h"
#include "proto/config.pb.h"

namespace PS {

static const int kGrpIDmax = 4096;

class TextParser {
 public:
  typedef DataConfig::TextFormat TextFormat;
  TextParser(TextFormat format, bool ignore_fea_grp = false);

  bool toProto(char* line, Instance* ins);
  InstanceInfo info();
  bool parseLibsvm(char*, Instance*);
  bool parseAdfea(char*, Instance*);
  bool parseTerafea(char*, Instance*);

 private:

  InstanceInfo info_;
  FeatureGroupInfo grp_info_[kGrpIDmax+1];
  bool ignore_fea_grp_;
  size_t num_ins_ = 0;

  typedef std::function<bool(char*, Instance*)> Convertor;
  Convertor convertor_;
};

}



// // assume the group_id has format   **...*aaaabbbbcc
// // then encode it as                ccbbbbaaaa00...0
// static bool encodeGroupID(uint64 in, uint64* out) {
//   if (in >= 1024) return false;
//   *out = (in << 62) | ((in & 0x3C) << 56) | ((in & 0x3C0) << 48);
//   return true;
// }
// static bool decodeGroupID(uint64 in, uint64* out) {
//   *out = (in >> 62) | (in >> 56 & 0x3C) | (in >> 48 & 0x3C0);
//   return true;
// }
// // use the first 10 bit to encode the group_id, the rest 54 for the feature
// // id. therefore, feature ranges for different groups are not overlapped.
// static bool encode(uint64 fea_id, uint64 group_id, uint64* out) {
//   uint64 g; if (!encodeGroupID(group_id, &g)) return false;
//   *out = ((fea_id << 10) >> 10) | g;
//   return true;
// }

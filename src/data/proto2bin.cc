#include "base/matrix_io.h"
#include "base/io.h"

DEFINE_string(input, "../config/data.config", "");
DEFINE_string(output, "../data/bin/xx", "");

int main(int argc, char *argv[]) {
  FLAGS_logtostderr = 1;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  using namespace PS;

  DataConfig cf;
  ReadFileToProtoOrDie(FLAGS_input, &cf);

  auto cf2 = searchFiles(cf);
  // LL << cf2.DebugString();
  auto data = readMatrices<double>(cf2);


  SArray<Key> key;
  auto X = data[1]->localize(&key);

  // InstanceInfo info;
  // for (int i = 0; i < cf2.file_size(); ++i) {
  //   InstanceInfo f;
  //   ReadFileToProtoOrDie(cf2.file(i)+".info", &f);
  //   info = i == 0 ? f : mergeInstanceInfo(info, f);
  // }

  // std::ofstream os(out+".X.fea_group");
  // for (int i = 0; i < info.individual_groups_size(); ++i) {
  //   auto g = info.individual_groups(i);
  //   os << g.group_id() << "\t" << g.feature_begin() << "\t" << g.feature_end();
  // }

  auto out = FLAGS_output;
  data[0]->writeToBinFile(out + ".y");
  X->writeToBinFile(out + ".X");
  key.writeToFile(SizeR::all(), out+".X.key");

  return 0;
}

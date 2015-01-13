#include "util/common.h"
#include "util/recordio.h"
// #include "proto/instance.pb.h"

DEFINE_int32(n, 3, "show the first *n* instances in text format");
DEFINE_string(input, "stdin", "stdin or input filename");

int main(int argc, char *argv[]) {
  FLAGS_logtostderr = 1;
  google::SetUsageMessage("./self -n num_instance < recordio_file");
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  using namespace PS;

  File* in = File::open(FLAGS_input, "r");
  RecordReader reader(in);

  for (int i = 0; i < FLAGS_n; ++i) {
    Instance ins;
    CHECK(reader.ReadProtocolMessage(&ins));
    std::cout << ins.ShortDebugString() << std::endl;
  }
  return 0;
}

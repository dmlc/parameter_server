#include "util/resource_usage.h"
#include "util/common.h"
#include "util/filelinereader.h"
#include "util/recordio.h"
#include "data/parse_text.h"

DEFINE_string(input, "stdin", "stdin or a input filename");
DEFINE_string(output, "part", "a output filename");

DEFINE_uint64(format_detector, 1000,
              "using the first *format_detector* lines to detect the format");
// DEFINE_bool(verbose, true, "");
DEFINE_string(format, "none", "pserver, libsvm, vw or adfea");

int main(int argc, char *argv[]) {
  using namespace PS;
  google::SetUsageMessage("./self -format libsvm < text_file > recordio_file");
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);


  string format = FLAGS_format;
  std::transform(format.begin(), format.end(), format.begin(), ::tolower);

  TextParser parser;
  if (format == "libsvm") {
    parser.setFormat(DataConfig::LIBSVM);
  } else if (format == "adfea") {
    parser.setFormat(DataConfig::ADFEA);
  }


  auto record_file = File::openOrDie(FLAGS_output+".recordio", "w");
  RecordWriter writer(record_file);

  Instance ins;
  int ignored = 0;
  FileLineReader reader(FLAGS_input.c_str());
  reader.set_line_callback([&parser, &ins, &writer, &ignored] (char *line) {
      ignored += !parser.toProtobuf(line, &ins);
      writer.WriteProtocolMessage(ins);
    });

  Timer t; t.start();
  reader.Reload();
  auto info = parser.info();
  writeProtoToASCIIFileOrDie(info, FLAGS_output+".info");
  t.stop();

  std::cerr << "written " << info.num_ins()
            << " instances in " <<  t.get()  << " sec." << std::endl;
  if (ignored) {
    std::cerr << ignored << " bad instances are skipped" << std::endl;
  }

  return 0;
}

#pragma once
namespace PS {

static void textToProto() {
  // TODO
  // string format = FLAGS_format;
  // std::transform(format.begin(), format.end(), format.begin(), ::tolower);

  // TextParser parser;
  // if (format == "libsvm") {
  //   parser.setFormat(DataConfig::LIBSVM);
  // } else if (format == "adfea") {
  //   parser.setFormat(DataConfig::ADFEA);
  // }


  // auto record_file = File::openOrDie(FLAGS_output+".recordio", "w");
  // RecordWriter writer(record_file);

  // Instance ins;
  // int ignored = 0;
  // FileLineReader reader(FLAGS_input.c_str());
  // reader.set_line_callback([&parser, &ins, &writer, &ignored] (char *line) {
  //     ignored += !parser.toProtobuf(line, &ins);
  //     writer.WriteProtocolMessage(ins);
  //   });

  // Timer t; t.start();
  // reader.Reload();
  // auto info = parser.info();
  // writeProtoToASCIIFileOrDie(info, FLAGS_output+".info");
  // t.stop();

  // std::cerr << "written " << info.num_ins()
  //           << " instances in " <<  t.get()  << " sec." << std::endl;
  // if (ignored) {
  //   std::cerr << ignored << " bad instances are skipped" << std::endl;
  // }
}
} // namespace PS

// // some utility functions about files
// #pragma once
// #include <dirent.h>
// #include <regex>

// #include "util/common.h"
// #include "proto/app.pb.h"
// #include "base/range.h"
// #include "util/split.h"
// #include "util/file.h"
// #include "util/hdfs.h"

// namespace PS {

// // static std::vector<DataConfig> assignDataToNodes(
// //     const DataConfig& data, int num_nodes) {
// //   CHECK_GT(data.file_size(), 0) << "search failed" << data.DebugString();
// //   CHECK_GE(data.file_size(), num_nodes) << "too many nodes";

// //   // evenly assign files to machines
// //   std::vector<DataConfig> nodes_config;
// //   for (int i = 0; i < num_nodes; ++i) {
// //     DataConfig dc; dc.set_format(DataConfig::PROTO);
// //     auto file_os = Range<int>(0, data.file_size()).evenDivide(num_nodes, i);
// //     for (int j = file_os.begin(); j < file_os.end(); ++j)
// //       dc.add_file(data.file(j));
// //     nodes_config.push_back(dc);
// //   }
// //   return nodes_config;
// // }


//   // if (data.format() == DataConfig::BIN) {
//   //   // format: Y, feature group 0, feature group 1, ...
//   //   // assume those data are shared by all workers, the first one is the label,
//   //   // and the second one is the training data

//   //   // while each of the rest present one feature group.
//   //   // FIXME how to store the
//   //   // feature group info
//   //   MatrixInfo info;
//   //   for (int i = 1; i < data.files_size(); ++i) {
//   //     ReadFileToProtoOrDie(data.files(i)+".info", &info);
//   //     global_training_info_.push_back(info);
//   //     global_training_feature_range_  =
//   //         global_training_feature_range_.setUnion(Range<Key>(info.col()));
//   //   }
//   //   SizeR global_data_range = SizeR(info.row());
//   //   for (int i = 0; i < num_worker; ++i) {
//   //     global_data_range.evenDivide(num_worker, i).to(data.mutable_range());
//   //     worker_training_.push_back(data);
//   //   }
//   // } else if (data.format() == DataConfig::PROTO) {
// } // namespace PS

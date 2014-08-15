// some utility functions about files
#pragma once
#include <dirent.h>
#include <regex>

#include "util/common.h"
#include "proto/app.pb.h"
#include "util/split.h"
#include "util/file.h"

namespace PS {

static std::vector<std::string> readFilenamesInDirectory(const std::string& directory) {
  std::vector<std::string> files;

  DIR *dir = opendir(directory.c_str());
  CHECK(dir != NULL) << "failed to open directory " << directory;
  struct dirent *ent;
  while ((ent = readdir (dir)) != NULL)
    files.push_back(string(ent->d_name));
  closedir (dir);
  return files;
}

namespace {

string filename(const string& full) {
  auto elems = split(full, '/');
  return elems.empty() ? "" : elems.back();
}

string path(const string& full) {
  auto elems = split(full, '/');
  if (elems.size() <= 1) return "";
  elems.pop_back();
  return join(elems, '/');
}

string removeExtension(const string& file) {
  auto elems = split(file, '.');
  if (elems.size() <= 1) return "";
  elems.pop_back();
  return join(elems, '.');
}

}

static DataConfig searchFiles(const DataConfig& config) {
  int n = config.file_size();
  CHECK_GE(n, 1) << "empty files: " << config.DebugString();

  std::vector<std::string> matched_files;

  for (int i = 0; i < n; ++i) {
    string dir = path(config.file(i));
    std::regex pattern;
    try {
      pattern = std::regex(filename(config.file(i)));
    } catch (const std::regex_error& e) {
      CHECK(false) << filename(config.file(i))
                   << " is not valid (supported) regex, regex_error caught: "
                   << e.what() << ". you may try gcc>=4.9 or llvm>=3.4";
    }

    // remove file extensions and duplications
    auto files = readFilenamesInDirectory(dir);
    for (auto& f : files) f = removeExtension(f);
    std::sort(files.begin(), files.end());
    auto it = std::unique(files.begin(), files.end());
    files.resize(std::distance(files.begin(), it));

    // match regex
    for (auto& f : files) {
      if (std::regex_match(f, pattern)) {
        matched_files.push_back(dir+"/"+f);
      }
    }
  }

  // remove duplicate files

  std::sort(matched_files.begin(), matched_files.end());
  auto it = std::unique(matched_files.begin(), matched_files.end());
  matched_files.resize(std::distance(matched_files.begin(), it));

  DataConfig ret = config;
  ret.clear_file();
  for (auto& f : matched_files)
    ret.add_file(f);
  return ret;
}

static std::vector<DataConfig> assignDataToNodes(
    const DataConfig& config, int num_nodes, InstanceInfo* info) {
  CHECK_EQ(config.format(), DataConfig::PROTO) << "TODO, support more formats";
  auto data = searchFiles(config);
  CHECK_GT(data.file_size(), 0) << "search failed" << config.DebugString();
  CHECK_GE(data.file_size(), num_nodes) << "too many nodes";

  for (int i = 0; i < data.file_size(); ++i) {
    InstanceInfo f;
    ReadFileToProtoOrDie(data.file(i)+".info", &f);
    *info = i == 0 ? f : mergeInstanceInfo(*info, f);
  }

  // evenly assign files to machines
  std::vector<DataConfig> nodes_config;
  for (int i = 0; i < num_nodes; ++i) {
    DataConfig dc; dc.set_format(DataConfig::PROTO);
    auto file_os = Range<int>(0, data.file_size()).evenDivide(num_nodes, i);
    for (int j = file_os.begin(); j < file_os.end(); ++j)
      dc.add_file(data.file(j));
    nodes_config.push_back(dc);
  }
  return nodes_config;
}


  // if (data.format() == DataConfig::BIN) {
  //   // format: Y, feature group 0, feature group 1, ...
  //   // assume those data are shared by all workers, the first one is the label,
  //   // and the second one is the training data

  //   // while each of the rest present one feature group.
  //   // FIXME how to store the
  //   // feature group info
  //   MatrixInfo info;
  //   for (int i = 1; i < data.files_size(); ++i) {
  //     ReadFileToProtoOrDie(data.files(i)+".info", &info);
  //     global_training_info_.push_back(info);
  //     global_training_feature_range_  =
  //         global_training_feature_range_.setUnion(Range<Key>(info.col()));
  //   }
  //   SizeR global_data_range = SizeR(info.row());
  //   for (int i = 0; i < num_worker; ++i) {
  //     global_data_range.evenDivide(num_worker, i).to(data.mutable_range());
  //     worker_training_.push_back(data);
  //   }
  // } else if (data.format() == DataConfig::PROTO) {
} // namespace PS

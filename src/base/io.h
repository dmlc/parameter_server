// some utility functions about files
#pragma once
#include <dirent.h>
#include <regex>

#include "util/common.h"
#include "proto/config.pb.h"
#include "util/split.h"

namespace PS {

std::vector<std::string> readFilenamesInDirectory(const std::string& directory) {
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

DataConfig searchFiles(const DataConfig& config) {
  int n = config.files_size();
  CHECK_GE(n, 1) << "empty files: " << config.DebugString();

  string dir;
  std::vector<std::regex> patterns;
  for (int i = 0; i < n; ++i) {
    if (i == 0)
      dir = path(config.files(i));
    else
      CHECK_EQ(dir, path(config.files(i)))
          << " all files should in the same directory";
    try {
      std::regex re(filename(config.files(i)));
      patterns.push_back(re);
    } catch (const std::regex_error& e) {
      CHECK(false) << filename(config.files(i))
                   << " is not valid (supported) regex, regex_error caught: " << e.what()
                   << ". you may try gcc4.9 or llvm5.1";
    }
  }

  // remove file extensions and duplications
  auto files = readFilenamesInDirectory(dir);
  for (auto& f : files) f = removeExtension(f);
  std::sort(files.begin(), files.end());
  auto it = std::unique(files.begin(), files.end());
  files.resize(std::distance(files.begin(), it));

  // match regex
  DataConfig ret = config;
  ret.clear_files();

  for (auto& f : files) {
    bool matched = false;
    for (auto& ex : patterns) {
      if (std::regex_match(f, ex)) {
        matched = true;
        break;
      }
    }
    if (matched) {
      ret.add_files(dir+"/"+f);
    }
  }

  return ret;
}

} // namespace PS

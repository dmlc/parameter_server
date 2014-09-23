#include "data/common.h"
#include <regex>
#include "util/file.h"

namespace PS {

DataConfig ithFile(const DataConfig& conf, int i, const string& suffix) {
  CHECK_GE(i, 0); CHECK_LT(i, conf.file_size());
  auto f = conf; f.clear_file(); f.add_file(conf.file(i) + suffix);
  return f;
}

InstanceInfo mergeInstanceInfo(const InstanceInfo& A, const InstanceInfo& B) {
  auto as = A.fea_grp_size();
  auto bs = B.fea_grp_size();
  if (!as) return B;
  if (!bs) return A;
  CHECK_EQ(as, bs);

  CHECK_EQ(A.label_type(), B.label_type());
  CHECK_EQ(A.fea_type(), B.fea_type());

  InstanceInfo C = A;
  C.set_num_ins(A.num_ins() + B.num_ins());
  C.set_nnz_ele(A.nnz_ele() + B.nnz_ele());
  C.clear_fea_grp();
  for (int i = 0; i < as; ++i) {
    auto G = A.fea_grp(i);
    G.set_nnz_ins(G.nnz_ins() + B.fea_grp(i).nnz_ins());
    G.set_nnz_ele(G.nnz_ele() + B.fea_grp(i).nnz_ele());
    G.set_fea_begin(std::min(G.fea_begin(), B.fea_grp(i).fea_begin()));
    G.set_fea_end(std::max(G.fea_end(), B.fea_grp(i).fea_end()));
    *C.add_fea_grp() = G;
  }
  return C;
}

DataConfig searchFiles(const DataConfig& config) {
  int n = config.file_size();
  CHECK_GE(n, 1) << "empty files: " << config.DebugString();
  std::vector<std::string> matched_files;
  for (int i = 0; i < n; ++i) {
    std::regex pattern;
    try {
      pattern = std::regex(getFilename(config.file(i)));
    } catch (const std::regex_error& e) {
      CHECK(false) << getFilename(config.file(i))
                   << " is not valid (supported) regex, regex_error caught: "
                   << e.what() << ". you may try gcc>=4.9 or llvm>=3.4";
    }
    auto dir = config; dir.clear_file();
    dir.add_file(getPath(config.file(i)));
    // match regex
    auto files = readFilenamesInDirectory(dir);
    for (auto& f : files) {
      if (std::regex_match(f, pattern)) {
        auto l = config.format() == DataConfig::TEXT ? f : removeExtension(f);
        matched_files.push_back(dir.file(0) + "/" + l);
      }
    }
  }
  // remove duplicate files
  std::sort(matched_files.begin(), matched_files.end());
  auto it = std::unique(matched_files.begin(), matched_files.end());
  matched_files.resize(std::distance(matched_files.begin(), it));
  DataConfig ret = config; ret.clear_file();
  for (auto& f : matched_files) ret.add_file(f);
  return ret;
}

std::vector<DataConfig> divideFiles(const DataConfig& data, int num) {
  CHECK_GT(data.file_size(), 0) << "empty files" << data.DebugString();
  CHECK_GE(data.file_size(), num) << "too many partitions";
  // evenly divide files
  std::vector<DataConfig> parts;
  for (int i = 0; i < num; ++i) {
    DataConfig dc = data; dc.clear_file();
    for (int j = 0; j < data.file_size(); ++j) {
      if (j % num == i) dc.add_file(data.file(j));
    }
    parts.push_back(dc);
  }
  return parts;
}

} // namespace PS



// inline FeatureGroupInfo
// mergeFeatureGroupInfo(const FeatureGroupInfo& A, const FeatureGroupInfo& B) {
//   auto C = A;
//   C.set_nnz(A.nnz() + B.nnz());
//   C.set_num_nonempty_ins(A.num_nonempty_ins() + B.num_nonempty_ins());
//   return C;
// }

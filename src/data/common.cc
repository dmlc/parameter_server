#include <regex>
#include "data/common.h"
#include "util/file.h"

namespace PS {

DEFINE_string(input, "stdin", "stdin or a filename");
DEFINE_string(output, "stdout", "stdout or a filename");
DEFINE_string(format, "none", "proto, pserver, libsvm, vw, adfea, or others");

DECLARE_bool(verbose);

DataConfig ithFile(const DataConfig& conf, int i, const string& suffix) {
  CHECK_GE(i, 0); CHECK_LT(i, conf.file_size());
  auto f = conf; f.clear_file(); f.add_file(conf.file(i) + suffix);
  return f;
}

MatrixInfo readMatrixInfo(
    const ExampleInfo& info, int slot_id, int sizeof_idx, int sizeof_val) {
  MatrixInfo f;
  int i = 0;
  for (; i < info.slot_size(); ++i) if (info.slot(i).id() == slot_id) break;
  if (i == info.slot_size()) return f;

  auto slot = info.slot(i);
  if (slot.format() == SlotInfo::DENSE) {
    f.set_type(MatrixInfo::DENSE);
  } else if (slot.format() == SlotInfo::SPARSE) {
    f.set_type(MatrixInfo::SPARSE);
  } else if (slot.format() == SlotInfo::SPARSE_BINARY) {
    f.set_type(MatrixInfo::SPARSE_BINARY);
  }
  f.set_row_major(true);
  f.set_id(slot.id());
  f.mutable_row()->set_begin(0);
  f.mutable_row()->set_end(info.num_ex());
  f.mutable_col()->set_begin(slot.min_key());
  f.mutable_col()->set_end(slot.max_key());

  f.set_nnz(slot.nnz_ele());
  f.set_sizeof_index(sizeof_idx);
  f.set_sizeof_value(sizeof_val);

  // LL << info.DebugString() << "\n" << f.DebugString();
  // *f.mutable_ins_info() = info;
  return f;
}

ExampleInfo mergeExampleInfo(const ExampleInfo& A, const ExampleInfo& B) {
  std::map<int, SlotInfo> slots;
  for (int i = 0; i < A.slot_size(); ++i) {
    slots[A.slot(i).id()] = A.slot(i);
  }

  for (int i = 0; i < B.slot_size(); ++i) {
    int id = B.slot(i).id();
    if (slots.count(id) == 0) { slots[id] = B.slot(i); continue; }
    auto a = slots[id];
    auto b = B.slot(i);
    CHECK_EQ(a.format(), b.format());
    a.set_min_key(std::min(a.min_key(), b.min_key()));
    a.set_max_key(std::max(a.max_key(), b.max_key()));
    a.set_nnz_ele(a.nnz_ele() + b.nnz_ele());
    a.set_nnz_ex(a.nnz_ex() + b.nnz_ex());
    slots[id] = a;
  }

  ExampleInfo C;
  C.set_num_ex(A.num_ex() + B.num_ex());
  for (const auto& it : slots) *C.add_slot() = it.second;
  return C;
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

    // list all files found in dir
    if (FLAGS_verbose) {
      size_t file_idx = 1;
      for (const auto& file : files) {
        LI << "All files found in [" << dir.file(0) << "]; [" <<
          file_idx++ << "/" << files.size() << "] [" << file << "]";
      }
    }

    for (auto& f : files) {
      if (std::regex_match(getFilename(f), pattern)) {
        auto l = config.format() == DataConfig::TEXT ? f : removeExtension(f);
        matched_files.push_back(dir.file(0) + "/" + getFilename(l));
      }
    }

    // list all matched files
    if (FLAGS_verbose) {
      size_t file_idx = 1;
      for (const auto& file : matched_files) {
        LI << "All matched files [" << file_idx++ << "/" << matched_files.size() <<
          "] [" << file << "]";
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
      int32 load_limit = data.max_num_files_per_worker();
      if (load_limit >= 0 && dc.file_size() >= load_limit) {
        break;
      }
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

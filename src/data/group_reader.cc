#include "data/group_reader.h"
#include "data/text_parser.h"
#include "util/threadpool.h"
#include "util/filelinereader.h"

DEFINE_bool(compress_cache, false, "");

namespace PS {

GroupReader::GroupReader(const DataConfig& cache) {
  if (cache.file_size()) dump_to_disk_ = true;
  cache_ = cache.file(0);
}

int GroupReader::read(const DataConfig& data, InstanceInfo* info) {
  CHECK(data.format() == DataConfig::TEXT);
  CHECK_GT(FLAGS_num_threads, 0);
  {
    FLAGS_num_threads = 2;
    ThreadPool pool(FLAGS_num_threads);
    for (int i = 0; i < data.file_size(); ++i) {
      auto one_file = ithFile(data, i);
      // readOneFile(one_file);
      pool.add([this, one_file](){ readOneFile(one_file); });
    }
    pool.startWorkers();
  }
  if (info) *info = info_;
}


bool GroupReader::readOneFile(const DataConfig& data) {
  TextParser parser(data.text(), data.ignore_fea_grp());
  SArray<float> label;
  struct Slot {
    SArray<float> val;
    SArray<uint64> col_idx;
    SArray<uint16> row_siz;
    bool writeToFile(const string& name) {
      return val.writeToFile(name+".val") && col_idx.writeToFile(name+".colidx")
          && row_siz.writeToFile(name+".rowsiz");
    }
  };

  Slot slots[kGrpIDmax+1];
  uint32 num_ins = 0;
  Instance ins;

  // first parse data into slots
  std::function<void(char*)> handle = [&] (char *line) {
    if (!parser.toProto(line, &ins)) return;
    // store them in slots
    slots[kGrpIDmax].val.pushBack(ins.label());
    for (int i = 0; i < ins.fea_grp_size(); ++i) {
      const auto& grp = ins.fea_grp(i);
      CHECK_LT(grp.grp_id(), kGrpIDmax);
      auto& slot = slots[grp.grp_id()];
      int fea_size = grp.fea_id_size();
      for (int j = 0; j < fea_size; ++j) {
        slot.col_idx.pushBack(grp.fea_id(j));
        if (grp.fea_val_size() == fea_size) slot.val.pushBack(grp.fea_val(j));
      }
      while (slot.row_siz.size() < num_ins) slot.row_siz.pushBack(0);
      slot.row_siz.pushBack(fea_size);
    }
    ++ num_ins;
  };
  FileLineReader reader(data.file(0));
  reader.set_line_callback(handle);
  reader.Reload();

  // save in cache
  string name = getFilename(data.file(0));
  for (int i = 0; i <= kGrpIDmax; ++i) {
    auto& slot = slots[i];
    if (i < kGrpIDmax && slot.row_siz.empty()) continue;
    CHECK(slot.writeToFile(cache_ + name + "_grp_" + std::to_string(i)));
  }

  auto info = parser.info();
  {
    Lock l(mu_);
    info_ = mergeInstanceInfo(info_, info);
  }
}

SArray<uint64> GroupReader::index(int grp_id) {

}

SArray<size_t> GroupReader::offset(int grp_id) {

}

SArray<float> GroupReader::value(int grp_id) {

}

} // namespace PS

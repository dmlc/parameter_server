#include "data/slot_reader.h"
#include "data/text_parser.h"
#include "data/info_parser.h"
#include "util/recordio.h"
#include "util/threadpool.h"
#include "util/filelinereader.h"
namespace PS {

void SlotReader::Init(const DataConfig& data, const DataConfig& cache) {
  // if (cache.file_size()) dump_to_disk_ = true;
  CHECK(cache.file_size());
  cache_ = cache.file(0);
  data_ = data;
}

string SlotReader::cacheName(const DataConfig& data, int slot_id) const {
  CHECK_GT(data.file_size(), 0);
  return cache_ + getFilename(data.file(0)) + "_slot_" + std::to_string(slot_id);
}

size_t SlotReader::nnzEle(int slot_id) const {
  size_t nnz = 0;
  for (int i = 0; i < info_.slot_size(); ++i) {
    if (info_.slot(i).id() == slot_id) nnz = info_.slot(i).nnz_ele();
  }
  return nnz;
}

int SlotReader::Read(ExampleInfo* info) {
  CHECK_GT(FLAGS_num_threads, 0);
  {
    Lock l(mu_);
    loaded_file_count_ = 0;
    num_ex_.resize(data_.file_size());
  }
  {
    // for (size_t i = 0; i < data_.file_size(); ++i) {
    //   VLOG(1) << "I will load data file [" << i + 1 << "/" <<
    //       data_.file_size() << "] [" << data_.file(i) << "]";
    // }

    ThreadPool pool(FLAGS_num_threads);
    for (int i = 0; i < data_.file_size(); ++i) {
      auto one_file = ithFile(data_, i);
      pool.add([this, one_file, i](){ readOneFile(one_file, i); });
    }
    pool.startWorkers();
  }
  if (info) *info = info_;
  for (int i = 0; i < info_.slot_size(); ++i) {
    slot_info_[info_.slot(i).id()] = info_.slot(i);
  }
  return 0;
}

bool SlotReader::readOneFile(const DataConfig& data, int ith_file) {
  mu_.lock();
  VLOG(1) << "loading data file [" << data.file(0) << "]; loaded [" <<
      loaded_file_count_ << "/" << data_.file_size() << "]";
  mu_.unlock();

  // check if hit cache
  string info_name = cache_ + getFilename(data.file(0)) + ".info";
  ExampleInfo info;
  if (readFileToProto(info_name, &info)) {
    // the data is already in cache_dir
    Lock l(mu_);
    info_ = mergeExampleInfo(info_, info);
    num_ex_[ith_file] = info.num_ex();
    return true;
  }

  InfoParser info_parser;
  struct VSlot {
    SArray<float> val;
    SArray<uint64> col_idx;
    SArray<uint16> row_siz;
    bool writeToFile(const string& name) {
      return val.CompressTo().WriteToFile(name+".value")
          && col_idx.CompressTo().WriteToFile(name+".colidx")
          && row_siz.CompressTo().WriteToFile(name+".rowsiz");
    }
  };
  VSlot vslots[kSlotIDmax];
  uint32 num_ex = 0;
  Example ex;

  // store ex in slots and also extract the info
  auto store = [&]() {
    if (!info_parser.add(ex)) return;
    for (int i = 0; i < ex.slot_size(); ++i) {
      const auto& slot = ex.slot(i);
      CHECK_LT(slot.id(), kSlotIDmax);
      auto& vslot = vslots[slot.id()];
      int key_size = slot.key_size();
      for (int j = 0; j < key_size; ++j) vslot.col_idx.push_back(slot.key(j));
      int val_size = slot.val_size();
      for (int j = 0; j < val_size; ++j) vslot.val.push_back(slot.val(j));
      while (vslot.row_siz.size() < num_ex) vslot.row_siz.push_back(0);
      vslot.row_siz.push_back(std::max(key_size, val_size));
    }
    ++ num_ex;
  };

  // read examples one by one
  if (data_.format() == DataConfig::TEXT) {
    ExampleParser text_parser;
    text_parser.Init(data.text(), data.ignore_feature_group());
    std::function<void(char*)> handle = [&] (char *line) {
      if (!text_parser.ToProto(line, &ex)) return;
      store();
    };
    FileLineReader reader(data);
    reader.set_line_callback(handle);
    reader.Reload();
  } else if (data_.format() == DataConfig::PROTO) {
    RecordReader reader(File::openOrDie(data, "r"));
    while (reader.ReadProtocolMessage(&ex)) {
      store();
    }
  } else {
    CHECK(false) << "unsupported format " << data_.DebugString();
  }

  // create the directory if necessary. but it seems stupid to check it each
  // time. a nature way is doing it at init(). but i'm afraid to it may be
  // problemic to have multiple processes doing it at the same time.
  if (!dirExists(getPath(info_name))) {
    createDir(getPath(info_name));
  }
  // save in cache
  info = info_parser.info();
  writeProtoToASCIIFileOrDie(info, info_name);
  for (int i = 0; i < kSlotIDmax; ++i) {
    auto& vslot = vslots[i];
    if (vslot.row_siz.empty() && vslot.val.empty()) continue;
    while (vslot.row_siz.size() < num_ex) vslot.row_siz.push_back(0);
    CHECK(vslot.writeToFile(cacheName(data, i)));
  }
  {
    Lock l(mu_);
    info_ = mergeExampleInfo(info_, info);
    loaded_file_count_++;
    num_ex_[ith_file] = num_ex;
    VLOG(1) << "loaded data file [" << data.file(0) << "]; loaded [" <<
        loaded_file_count_ << "/" << data_.file_size() << "]";
  }
  return true;
}

SArray<uint64> SlotReader::index(int slot_id) {
  auto nnz = nnzEle(slot_id);
  if (nnz == 0) return SArray<uint64>();
  SArray<uint64> idx = index_cache_[slot_id];
  if (idx.size() == nnz) return idx;
  for (int i = 0; i < data_.file_size(); ++i) {
    string file = cacheName(ithFile(data_, i), slot_id) + ".colidx";
    SArray<char> comp;
    if (!comp.ReadFromFile(file)) continue;
    SArray<uint64> uncomp; uncomp.UncompressFrom(comp);
    idx.append(uncomp);
  }
  CHECK_EQ(idx.size(), nnz);
  index_cache_[slot_id] = idx;
  return idx;
}

SArray<size_t> SlotReader::offset(int slot_id) {
  if (offset_cache_[slot_id].size() == info_.num_ex()+1) {
    return offset_cache_[slot_id];
  }
  SArray<size_t> os(1); os[0] = 0;
  if (nnzEle(slot_id) == 0) return os;
  for (int i = 0; i < data_.file_size(); ++i) {
    string file = cacheName(ithFile(data_, i), slot_id) + ".rowsiz";
    SArray<char> comp;
    SArray<uint16> uncomp;
    if (comp.ReadFromFile(file) && !comp.empty()) {
      uncomp.UncompressFrom(comp);
      CHECK_EQ(uncomp.size(), num_ex_[i]) << file;
    } else {
      uncomp.resize(num_ex_[i], 0);
    }
    size_t n = os.size();
    os.resize(n + uncomp.size());
    for (size_t i = 0; i < uncomp.size(); ++i) os[i+n] = os[i+n-1] + uncomp[i];
  }
  CHECK_EQ(os.size(), info_.num_ex()+1);
  offset_cache_[slot_id] = os;
  return os;
}

} // namespace PS

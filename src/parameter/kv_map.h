#pragma once
#include "ps.h"
#include "parameter/parameter.h"
namespace PS {

/**
 * @brief Default entry type for KVMap
 */
template<typename V>
struct KVMapEntry {
  void Get(V* data, void* state) { *data = value; }
  void Set(const V* data, void* state) { value = *data; }
  V value;
};

/**
 * @brief Default state type for KVMap
 */
struct KVMapState {
  void Update() { }
};

/**
 * @brief A key-value store with fixed length value.
 *
 *
 * @tparam K the key type
 * @tparam V the value type
 * @tparam E the entry type
 * @tparam S the state type
 */
template <typename K, typename V,
          typename E = KVMapEntry<V>,
          typename S = KVMapState>
class KVMap : public Parameter {
 public:
  /**
   * @brief Constructor
   *
   * @param k the length of a value entry
   * @param id customer id
   */
  KVMap(int k = 1, int id = NextCustomerID()) :
      Parameter(id), k_(k) {
    CHECK_GT(k, 0);
  }
  virtual ~KVMap() { }

  void set_state(const S& s) { state_ = s; }

  virtual void Slice(const Message& request, const std::vector<Range<Key>>& krs,
                     std::vector<Message*>* msgs) {
    SliceKOFVMessage<K>(request, krs, msgs);
  }

  virtual void GetValue(Message* msg);
  virtual void SetValue(const Message* msg);

  virtual void WriteToFile(std::string file);

 protected:
  int k_;
  S state_;
  // TODO use multi-thread cuokoo hash
  std::unordered_map<K, E> data_;
};

template <typename K, typename V, typename E, typename S>
void KVMap<K,V,E,S>::GetValue(Message* msg) {
  SArray<K> key(msg->key);
  size_t n = key.size();
  SArray<V> val(n * k_);
  for (size_t i = 0; i < n; ++i) {
    data_[key[i]].Get(val.data() + i * k_, &state_);
  }
  msg->add_value(val);
}

template <typename K, typename V, typename E, typename S>
void KVMap<K,V,E,S>::SetValue(const Message* msg) {
  SArray<K> key(msg->key);
  size_t n = key.size();
  CHECK_EQ(msg->value.size(), 1);
  SArray<V> val(msg->value[0]);
  CHECK_EQ(n * k_, val.size());

  for (size_t i = 0; i < n; ++i) {
    data_[key[i]].Set(val.data() + i * k_, &state_);
  }
  state_.Update();
}
#if USE_S3
bool s3file(const std::string& name);
std::string s3Prefix(const std::string& path);
std::string s3Bucket(const std::string& path);
std::string s3FileUrl(const std::string& path);
#endif // USE_S3

template <typename K, typename V, typename E, typename S>
void KVMap<K,V,E,S>::WriteToFile(std::string file) {
#if USE_S3
  std::string s3_file;
  if (s3file(file)) {
    s3_file=file;
    // create a local model dir
    file=s3Prefix(s3_file);
  }
#endif // USE_S3
  if (!dirExists(getPath(file))) {
    createDir(getPath(file));
  }
  std::ofstream out(file); CHECK(out.good());
  V v;
  for (auto& e : data_) {
    e.second.Get(&v, &state_);
    if (v != 0) out << e.first << "\t" << v << std::endl;
  }
#if USE_S3
  if (s3file(s3_file)) {
    // upload model
    std::string cmd = "curl -s '"+s3FileUrl(s3_file)+"?Content-Length="
    +std::to_string(File::size(file))+"&x-amz-acl=public-read' --upload-file "+file;
    LOG(INFO)<<cmd;
    system(cmd.c_str());
    // remove local model
    cmd="rm -rf "+file;
    system(cmd.c_str());
  }
#endif // USE_S3
}


}  // namespace PS

#include "util/mail.h"

namespace PS {

bool Mail::Serialization(std::string *data) {
  std::string sync_flag_str;
  CHECK(flag_.SerializeToString(&sync_flag_str));
  int64 sync_flag_size = sync_flag_str.length();
  std::string keys_data;
  CHECK(keys_.Serialization(&keys_data));
  int64 keys_size = keys_data.length();
  std::string vals_data;
  CHECK(vals_.Serialization(&vals_data));
  int64 vals_size = vals_data.length();
  // first copy key and value to string 
  // its not efficient, will use memcopy later
  std::string sync_flag_size_str((const char *)&sync_flag_size, sizeof(sync_flag_size));
  std::string keys_size_str((const char *)&keys_size, sizeof(keys_size));
  std::string vals_size_str((const char *)&vals_size, sizeof(vals_size));
  *data = sync_flag_size_str + sync_flag_str + keys_size_str + keys_data + vals_size_str + vals_data;
  CHECK_EQ(data->size(), sync_flag_size + keys_size + vals_size + sizeof(int64)*3);
  return true;
}

char *Mail::ParseFromString(char *data, char *end_of_data) {
  if (data >= end_of_data) {
    LOG(WARNING) << "data == end_of_data";
    return data;
  }
  // get sync_flag_size
  char *p = data;
  int64 sync_flag_size = *(int64*)p;
  p += sizeof(int64);
  if ((p+sync_flag_size) > end_of_data) {
    LOG(WARNING) << "incomplete sync_flag data";
    return data;
  }
  std::string sync_flag_str(p, sync_flag_size);
  CHECK(flag_.ParseFromString(sync_flag_str));
  p += sync_flag_size;
  if ((p+sizeof(int64)) > end_of_data) {
    LOG(WARNING) << "incomplete keys_size";
    return data;
  }
  int64 keys_size = *(int64*)p;
  p += sizeof(int64);
  if ((p+keys_size) > end_of_data) {
    LOG(WARNING) << "incomplete keys data";
    return data;
  }
  CHECK(keys_.ParseFromString(p));
  p += keys_size;
  if ((p+sizeof(int64)) > end_of_data) {
    LOG(WARNING) << "incomplete vals_size";
    return data;
  }
  int64 vals_size = *(int64*)p;
  p += sizeof(int64);
  if ((p+vals_size) > end_of_data) {
    LOG(WARNING) << "incomplete keys data";
    return data;
  }
  CHECK(vals_.ParseFromString(p));
  p += vals_size;
  return p;
}

} // namespace PS

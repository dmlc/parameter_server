#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <iostream>
#include <memory>
#include <dirent.h>
#include <regex>

#include "util/file.h"
#include "util/split.h"

// TODO read and write gz files, see zlib.h. evaluate the performace gain
namespace PS {

File* File::open(const std::string& name, const char* const flag) {
  File* f;
  if (name == "stdin") {
    f = new File(stdin, name);
  } else if (name == "stdout") {
    f = new File(stdout, name);
  } else if (name == "stderr") {
    f = new File(stderr, name);
  } else {
    FILE* des = fopen(name.data(), flag);
    if (des == NULL) {
      LOG(ERROR) << "cannot open " << name;
      return NULL;
    }
    f = new File(des, name);
  }
  return f;
}

File* File::openOrDie(const std::string& name, const char* const flag) {
  File* f = File::open(name, flag);
  CHECK(f != NULL && f->open());
  return f;
}

File* File::open(const DataConfig& name,  const char* const flag) {
  CHECK_EQ(name.file_size(), 1);
  auto filename = name.file(0);
  if (name.has_hdfs()) {
    string cmd = hadoopFS(name.hdfs()) + " -cat " + filename;
    FILE* des = popen(cmd.c_str(), "r");
    auto f = new File(des, filename);
    return f;
  } else {
    return open(filename, flag);
  }
}
File* File::openOrDie(const DataConfig& name,  const char* const flag) {
  File* f = open(name, flag);
  CHECK(f != NULL && f->open());
  return f;
}

size_t File::size(const std::string& name) {
  struct stat f_stat;
  stat(name.c_str(), &f_stat);
  return f_stat.st_size;
}

size_t File::size() {
  return File::size(name_);
}

// bool File::Flush() { return is_gz_ ? gzflush()fflush(f_) == 0; }

bool File::close() {
  if (fclose(f_) == 0) {
    f_ = NULL;
    return true;
  } else {
    return false;
  }
}

size_t File::read(void* const buf, size_t size) {
  return fread(buf, 1, size, f_);
}

size_t File::write(const void* const buf, size_t size) {
  return fwrite(buf, 1, size, f_);
}


char* File::readLine(char* const output, uint64 max_length) {
  return fgets(output, max_length, f_);
}

bool File::seek(size_t position) {
  return (fseek(f_, position, SEEK_SET) == 0);
}

int64 File::readToString(std::string* const output, uint64 max_length) {
  CHECK_NOTNULL(output);
  output->clear();

  if (max_length == 0) return 0;
 // if (max_length < 0) return -1;

  int64 needed = max_length;
  int bufsize = (needed < (2 << 20) ? needed : (2 << 20));

  std::unique_ptr<char[]> buf(new char[bufsize]);

  int64 nread = 0;
  while (needed > 0) {
    nread = read(buf.get(), (bufsize < needed ? bufsize : needed));
    if (nread > 0) {
      output->append(buf.get(), nread);
      needed -= nread;
    } else {
      break;
    }
  }
  return (nread >= 0 ? static_cast<int64>(output->size()) : -1);
}

size_t File::writeString(const std::string& line) {
  return write(line.c_str(), line.size());
}

// bool File::WriteLine(const std::string& line) {
//   if (Write(line.c_str(), line.size()) != line.size()) return false;
//   return Write("\n", 1) == 1;
// }

/////////////////////////////////////////

bool readFileToString(const std::string& file_name, std::string* output) {
  File* file = File::open(file_name, "r");
  if (file == NULL) return false;
  size_t size = file->size();
  return (size <= file->readToString(output, size*100));
}

bool writeStringToFile(const std::string& data, const std::string& file_name) {
  File* file = File::open(file_name, "w");
  if (file == NULL) return false;
  return (file->writeString(data) == data.size() && file->close());
}

namespace {
class NoOpErrorCollector : public google::protobuf::io::ErrorCollector {
 public:
  virtual void AddError(int line, int column, const std::string& message) {}
};
}  // namespace


bool readFileToProto(const std::string& file_name, GProto* proto) {
  DataConfig conf; conf.add_file(file_name);
  return readFileToProto(conf, proto);
}

bool readFileToProto(const DataConfig& name, GProto* proto) {
  File* f = File::open(name, "r");
  if (f == NULL) return false;
  size_t size = 100000;
  if (!name.has_hdfs()) size = f->size();
  std::string str; f->readToString(&str, size*100);

  // Attempt to decode ASCII before deciding binary. Do it in this order because
  // it is much harder for a binary encoding to happen to be a valid ASCII
  // encoding than the other way around. For instance "index: 1\n" is a valid
  // (but nonsensical) binary encoding. We want to avoid printing errors for
  // valid binary encodings if the ASCII parsing fails, and so specify a no-op
  // error collector.
  NoOpErrorCollector error_collector;
  google::protobuf::TextFormat::Parser parser;
  parser.RecordErrorsTo(&error_collector);
  if (parser.ParseFromString(str, proto)) {
    return true;
  }
  if (proto->ParseFromString(str)) {
    return true;
  }
  // Re-parse the ASCII, just to show the diagnostics (we could also get them
  // out of the ErrorCollector but this way is easier).
  google::protobuf::TextFormat::ParseFromString(str, proto);
  LOG(ERROR) << "Could not parse contents of " << name.DebugString();
  return false;
}

void readFileToProtoOrDie(
    const DataConfig& name, GProto* proto) {
  CHECK(readFileToProto(name, proto));
}

void readFileToProtoOrDie(
    const std::string& file_name, GProto* proto) {
  CHECK(readFileToProto(file_name, proto)) << "file_name: " << file_name;
}

bool writeProtoToASCIIFile(
    const GProto& proto, const std::string& file_name) {
  std::string proto_string;
  return google::protobuf::TextFormat::PrintToString(proto, &proto_string) &&
         writeStringToFile(proto_string, file_name);
}

void writeProtoToASCIIFileOrDie(
    const GProto& proto, const std::string& file_name) {
  CHECK(writeProtoToASCIIFile(proto, file_name)) << "file_name: " << file_name;
}

bool writeProtoToFile(
    const GProto& proto, const std::string& file_name) {
  std::string proto_string;
  return proto.AppendToString(&proto_string) &&
      writeStringToFile(proto_string, file_name);
}

void writeProtoToFileOrDie(const GProto& proto,
                           const std::string& file_name) {
  CHECK(writeProtoToFile(proto, file_name)) << "file_name: " << file_name;
}

// TODO read home from $HDFS_HOME if empty
std::string hadoopFS(const HDFSConfig& conf) {
  return (conf.home() + "/bin/hadoop dfs -D fs.default.name=" + conf.namenode()
          + " -D hadoop.job.ugi=" + conf.ugi());
}


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

std::vector<std::string> readFilenamesInDirectory(const DataConfig& directory) {
  CHECK_EQ(directory.file_size(), 1);
  auto dirname = directory.file(0);
  if (!directory.has_hdfs()) {
    return readFilenamesInDirectory(dirname);
  }
  // read hdfs directory
  std::vector<std::string> files;
  string cmd = hadoopFS(directory.hdfs()) + " -ls " + dirname;
  FILE* des = popen(cmd.c_str(), "r"); CHECK(des);
  char line[10000];
  while (fgets(line, 10000, des)) {
    auto ents = split(std::string(line), ' ', true);
    if (ents.size() != 8) continue;
    if (ents[0][0] == 'd') continue;
    files.push_back(ents.back());
  }
  pclose(des);
  return files;
}

namespace {
string filename(const string& full) {
  auto elems = split(full, '/');
  return elems.empty() ? "" : elems.back();
}
string path(const string& full) {
  auto elems = split(full, '/');
  if (elems.size() <= 1) return full;
  elems.pop_back();
  return join(elems, '/');
}
string removeExtension(const string& file) {
  auto elems = split(file, '.');
  if (elems.size() <= 1) return file;
  elems.pop_back();
  return join(elems, '.');
}
}

DataConfig searchFiles(const DataConfig& config) {
  int n = config.file_size();
  CHECK_GE(n, 1) << "empty files: " << config.DebugString();
  std::vector<std::string> matched_files;
  for (int i = 0; i < n; ++i) {
    std::regex pattern;
    try {
      pattern = std::regex(filename(config.file(i)));
    } catch (const std::regex_error& e) {
      CHECK(false) << filename(config.file(i))
                   << " is not valid (supported) regex, regex_error caught: "
                   << e.what() << ". you may try gcc>=4.9 or llvm>=3.4";
    }
    auto dir = config; dir.clear_file();
    dir.add_file(path(config.file(i)));
    // match regex
    auto files = readFilenamesInDirectory(dir);
    for (auto& f : files) {
      if (std::regex_match(f, pattern)) {
        matched_files.push_back(dir.file(0) + "/" + removeExtension(f));
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


// an old version which support read and write gz files

// File* File::open(const std::string& name, const char* const flag) {
//   File* f;
//   if (name == "stdin") {
//     f = new File(stdin, name);
//   } else if (name == "stdout") {
//     f = new File(stdout, name);
//   } else if (name == "stderr") {
//     f = new File(stderr, name);
//   } else if (name.size() > 3 && std::string(name.end()-3, name.end()) == ".gz") {
//     gzFile des = gzopen(name.data(), flag);
//     if (des == NULL) {
//       LOG(ERROR) << "cannot open " << name;
//       return NULL;
//     }
//     f = new File(des, name);
//   } else {
//     FILE*  des = fopen(name.data(), flag);
//     if (des == NULL) {
//       LOG(ERROR) << "cannot open " << name;
//       return NULL;
//     }
//     f = new File(des, name);
//   }
//   return f;
// }

// File* File::openOrDie(const std::string& name, const char* const flag) {
//   File* f = File::open(name, flag);
//   CHECK(f != NULL && f->open());
//   return f;
// }

// File* File::open(const DataConfig& name,  const char* const flag) {
//   CHECK_EQ(name.file_size(), 1);
//   auto filename = name.file(0);
//   if (name.has_hdfs()) {
//     string cmd = hadoopFS(name.hdfs()) + " -cat " + filename;
//     FILE* des = popen(cmd.c_str(), "r");
//     auto f = new File(des, filename);
//     return f;
//   } else {
//     return open(filename, flag);
//   }
// }
// File* File::openOrDie(const DataConfig& name,  const char* const flag) {
// File* f = open(name, flag);
// CHECK(f != NULL && f->open());
// return f;
// }

// size_t File::size(const std::string& name) {
//   struct stat f_stat;
//   stat(name.c_str(), &f_stat);
//   return f_stat.st_size;
// }

// size_t File::size() {
//   return File::size(name_);
// }

// // bool File::Flush() { return is_gz_ ? gzflush()fflush(f_) == 0; }

// bool File::close() {
//   if (gz_f_) {
//     if (gzclose(gz_f_) == Z_OK) {
//       gz_f_ = NULL;
//       return true;
//     } else {
//       return false;
//     }
//   }
//   if (f_) {
//     if (fclose(f_) == 0) {
//       f_ = NULL;
//       return true;
//     } else {
//       return false;
//     }
//   }
//   return true;
// }

// size_t File::read(void* const buf, size_t size) {
//   return (is_gz_ ? gzread(gz_f_, buf, size) : fread(buf, 1, size, f_));
// }

// size_t File::write(const void* const buf, size_t size) {
//   return (is_gz_ ? gzwrite(gz_f_, buf, size) : fwrite(buf, 1, size, f_));
// }


// char* File::readLine(char* const output, uint64 max_length) {
//   return (is_gz_ ? gzgets(gz_f_, output, max_length) : fgets(output, max_length, f_));
// }

// bool File::seek(size_t position) {
//   return (is_gz_ ?
//           gzseek(gz_f_, position, SEEK_SET) == position :
//           fseek(f_, position, SEEK_SET) == 0);
// }

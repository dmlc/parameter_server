#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <iostream>
#include <memory>

#include "util/file.h"

// TODO read and write gz files, see zlib.h. evaluate the performace gain
namespace PS {

File* File::open(const std::string& name, const char* const flag) {
  File* f;
  if (name.size() > 3 && std::string(name.end()-3, name.end()) == ".gz") {
    gzFile des = gzopen(name.data(), flag);
    if (des == NULL) {
      LOG(ERROR) << "cannot open " << name;
      return NULL;
    }
    f = new File(NULL, des, name);
  } else {
    FILE*  des = fopen(name.data(), flag);
    if (des == NULL) {
      LOG(ERROR) << "cannot open " << name;
      return NULL;
    }
    f = new File(des, NULL, name);
  }
  return f;
}

File* File::openOrDie(const std::string& name, const char* const flag) {
  File* f = File::open(name, flag);
  CHECK(f != NULL && f->open());
  return f;
}

size_t File::size() {
  struct stat f_stat;
  stat(name_.c_str(), &f_stat);
  return f_stat.st_size;
}

// bool File::Flush() { return is_gz_ ? gzflush()fflush(f_) == 0; }

bool File::Close() {
  if (gz_f_) {
    if (gzclose(gz_f_) == Z_OK) {
      gz_f_ = NULL;
      return true;
    } else {
      return false;
    }
  }
  if (f_) {
    if (fclose(f_) == 0) {
      f_ = NULL;
      return true;
    } else {
      return false;
    }
  }
  return true;
}

size_t File::Read(void* const buf, size_t size) {
  return (is_gz_ ? gzread(gz_f_, buf, size) : fread(buf, 1, size, f_));
}

size_t File::Write(const void* const buf, size_t size) {
  return (is_gz_ ? gzwrite(gz_f_, buf, size) : fwrite(buf, 1, size, f_));
}


char* File::ReadLine(char* const output, uint64 max_length) {
  return (is_gz_ ? gzgets(gz_f_, output, max_length) : fgets(output, max_length, f_));
}

bool File::seek(size_t position) {
  return (is_gz_ ?
          gzseek(gz_f_, position, SEEK_SET) == position :
          fseek(f_, position, SEEK_SET) == 0);
}

int64 File::ReadToString(std::string* const output, uint64 max_length) {
  CHECK_NOTNULL(output);
  output->clear();

  if (max_length == 0) return 0;
 // if (max_length < 0) return -1;

  int64 needed = max_length;
  int bufsize = (needed < (2 << 20) ? needed : (2 << 20));

  std::unique_ptr<char[]> buf(new char[bufsize]);

  int64 nread = 0;
  while (needed > 0) {
    nread = Read(buf.get(), (bufsize < needed ? bufsize : needed));
    if (nread > 0) {
      output->append(buf.get(), nread);
      needed -= nread;
    } else {
      break;
    }
  }
  return (nread >= 0 ? static_cast<int64>(output->size()) : -1);
}

size_t File::WriteString(const std::string& line) {
  return Write(line.c_str(), line.size());
}

// bool File::WriteLine(const std::string& line) {
//   if (Write(line.c_str(), line.size()) != line.size()) return false;
//   return Write("\n", 1) == 1;
// }

/////////////////////////////////////////

bool ReadFileToString(const std::string& file_name, std::string* output) {
  File* file = File::open(file_name, "r");
  if (file == NULL) return false;
  size_t size = file->size();
  return (size <= file->ReadToString(output, size*100));
}

bool WriteStringToFile(const std::string& data, const std::string& file_name) {
  File* file = File::open(file_name, "w");
  if (file == NULL) return false;
  return (file->WriteString(data) == data.size() && file->Close());
}

namespace {
class NoOpErrorCollector : public google::protobuf::io::ErrorCollector {
 public:
  virtual void AddError(int line, int column, const std::string& message) {}
};
}  // namespace

bool ReadFileToProto(const std::string& file_name, google::protobuf::Message* proto) {
  std::string str;
  if (!ReadFileToString(file_name, &str)) {
    LOG(ERROR) << "Could not read " << file_name;
    return false;
  }
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
  LOG(ERROR) << "Could not parse contents of " << file_name;
  return false;
}

void ReadFileToProtoOrDie(const std::string& file_name, google::protobuf::Message* proto) {
  CHECK(ReadFileToProto(file_name, proto)) << "file_name: " << file_name;
}

bool WriteProtoToASCIIFile(const google::protobuf::Message& proto,
                           const std::string& file_name) {
  std::string proto_string;
  return google::protobuf::TextFormat::PrintToString(proto, &proto_string) &&
         WriteStringToFile(proto_string, file_name);
}

void WriteProtoToASCIIFileOrDie(const google::protobuf::Message& proto,
                                const std::string& file_name) {
  CHECK(WriteProtoToASCIIFile(proto, file_name)) << "file_name: " << file_name;
}

bool WriteProtoToFile(const google::protobuf::Message& proto, const std::string& file_name) {
  std::string proto_string;
  return proto.AppendToString(&proto_string) &&
         WriteStringToFile(proto_string, file_name);
}

void WriteProtoToFileOrDie(const google::protobuf::Message& proto,
                           const std::string& file_name) {
  CHECK(WriteProtoToFile(proto, file_name)) << "file_name: " << file_name;
}


} // namespace PS

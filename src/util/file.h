#pragma once

#include <cstdlib>
#include <cstdio>
#include <string>
#include "util/integral_types.h"
#include "glog/logging.h"

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/io/tokenizer.h"

namespace PS {

class File {
 public:
  // Opens file "name" with flags specified by "flag".
  // Flags are defined by fopen(), that is "r", "r+", "w", "w+". "a", and "a+".
  static File* Open(const char* const name, const char* const flag);
  static File* Open(const std::string& name, const char* const flag) {
    return Open(name.c_str(), flag);
  }

  // Opens file "name" with flags specified by "flag"
  // If open failed, program will exit.
  static File* OpenOrDie(const char* const name, const char* const flag);
  static File* OpenOrDie(const std::string& name, const char* const flag) {
    return OpenOrDie(name.c_str(), flag);
  }

  // Reads "size" bytes to buff from file, buff should be pre-allocated.
  size_t Read(void* const buff, size_t size);

  // Reads "size" bytes to buff from file, buff should be pre-allocated.
  // If read failed, program will exit.
  void ReadOrDie(void* const buff, size_t size);

  // Reads a line from file to a std::string.
  // Each line must be no more than max_length bytes
  char* ReadLine(char* const output, uint64 max_length);

  // Reads the whole file to a std::string, with a maximum length of 'max_length'.
  // Returns the number of bytes read.
  int64 ReadToString(std::string* const line, uint64 max_length);

  // Writes "size" bytes of buff to file, buff should be pre-allocated.
  size_t Write(const void* const buff, size_t size);

  // Writes "size" bytes of buff to file, buff should be pre-allocated.
  // If write failed, program will exit.
  void WriteOrDie(const void* const buff, size_t size);

  // Writes a std::string to file.
  size_t WriteString(const std::string& line);

  // Writes a std::string to file and append a "\n".
  bool WriteLine(const std::string& line);

  // Closes the file.
  bool Close();

  // Flushes buffer.
  bool Flush();

  // Returns file size.
  size_t Size();

  // current file position
  size_t position() {
    return (size_t) ftell(f_);
  }

  // seek a position, starting from the head
  bool seek(size_t position) {
    return (fseek(f_, position, SEEK_SET) == 0);
  }

  // Returns the file name.
  std::string filename() const;

  // Deletes a file.
  static bool Delete(const char* const name);

  // Tests if a file exists.
  static bool Exists(const char* const name);

  bool Open() const;

  // protobuf
  bool WriteASCIIProto(const google::protobuf::Message& proto) {
    std::string proto_string;
    return google::protobuf::TextFormat::PrintToString(proto, &proto_string) &&
        (WriteString(proto_string) == proto_string.size());
  }

 private:
  File(FILE* const descriptor, const std::string& name);

  FILE* f_;
  const std::string name_;
};

bool ReadFileToString(const std::string& file_name, std::string* output);
bool WriteStringToFile(const std::string& data, const std::string& file_name);

// convenient functions dealing with protobuf
bool ReadFileToProto(const std::string& file_name, google::protobuf::Message* proto);
void ReadFileToProtoOrDie(const std::string& file_name, google::protobuf::Message* proto);

bool WriteProtoToASCIIFile(const google::protobuf::Message& proto,
                           const std::string& file_name);
void WriteProtoToASCIIFileOrDie(const google::protobuf::Message& proto,
                                const std::string& file_name);
bool WriteProtoToFile(const google::protobuf::Message& proto, const std::string& file_name);
void WriteProtoToFileOrDie(const google::protobuf::Message& proto,
                           const std::string& file_name);

} // namespace PS

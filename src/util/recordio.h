#pragma once

#include <string>
#include <memory>
#include "util/file.h"
namespace PS {

namespace {
static const int kMagicNumber = 0x3ed7230a;
}

// This class appends a protocol buffer to a file in a binary format.
// The data written in the file follows the following format (sequentially):
// - MagicNumber (32 bits) to recognize this format.
// - data payload size (32 bits)
// - Payload
class RecordWriter {
 public:
  // Magic number when writing and reading protocol buffers.

  explicit RecordWriter(File* const file) : file_(file) { }
  bool Close() { return file_->Close(); }

  template <class P> bool WriteProtocolMessage(const P& proto) {
    std::string buffer;
    proto.SerializeToString(&buffer);
    const uint32 buff_size = (uint32) buffer.size();
    if (file_->Write(&kMagicNumber, sizeof(kMagicNumber)) !=
        sizeof(kMagicNumber)) {
      return false;
    }
    if (file_->Write(&buff_size, sizeof(buff_size)) != sizeof(buff_size)) {
      return false;
    }
    if (file_->Write(buffer.c_str(), buff_size) != buff_size) {
      return false;
    }
    return true;
  }

 private:
  File* const file_;
};

// This class reads a protocol buffer from a file.
// The format must be the one described in RecordWriter, above.
class RecordReader {
 public:
  explicit RecordReader(File* const file) : file_(file) { }
  bool Close() { return file_->Close(); }

  template <class P> bool ReadProtocolMessage(P* const proto) {
    uint32 size = 0;
    int magic_number = 0;

    if (file_->Read(&magic_number, sizeof(magic_number)) !=
        sizeof(magic_number)) {
      return false;
    }
    if (magic_number != kMagicNumber) {
      return false;
    }
    if (file_->Read(&size, sizeof(size)) != sizeof(size)) {
      return false;
    }
    std::unique_ptr<char[]> buffer(new char[size + 1]);
    if (file_->Read(buffer.get(), size) != size) {
      return false;
    }
    proto->ParseFromArray(buffer.get(), size);
    return true;
  }

 private:
  File* const file_;
};

}  // namespace PS

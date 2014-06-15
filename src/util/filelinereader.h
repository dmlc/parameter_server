#pragma once

#include <cstdlib>
#include <cstdio>
#include <string>
#include <functional>

#include "util/file.h"

namespace PS {

// The FileLineReader class will read a text file specified by
// 'filename' line by line.  Each line will be cleaned with respect to
// termination ('\n' and '\r').  The line callback will be called in
// sequence on each line.
class FileLineReader {
 public:
  // Creates a file line reader object that will read the file 'filename'
  // line by line.
  explicit FileLineReader(const char* const filename)
      : filename_(filename), loaded_successfully_(false) { }

  ~FileLineReader() { }

  // Sets the line callback and takes ownership.
  void set_line_callback(std::function<void(char*)> callback) {
    line_callback_ = callback;
  }

  // Reloads the file line by line.
  void Reload();

  // Indicates if the file was loaded successfully.
  bool loaded_successfully() const { return loaded_successfully_; }

 private:
  const char* filename_;
  std::function<void(char*)> line_callback_;
  bool loaded_successfully_;
};

}  // namespace PS

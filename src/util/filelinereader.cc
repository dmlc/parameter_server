#include "util/filelinereader.h"

#include <cstring>
#include <string>
#include <memory>

#include "util/file.h"
#include "glog/logging.h"

namespace PS {

void FileLineReader::Reload() {
  const int kMaxLineLength = 60 * 1024;
  File* const data_file = File::open(filename_, "r");
  if (data_file == NULL) {
    loaded_successfully_ = false;
    return;
  }

  std::unique_ptr<char[]> line(new char[kMaxLineLength]);
  for (;;) {
    char* const result = data_file->ReadLine(line.get(), kMaxLineLength);
    if (result == NULL) {
      data_file->Close();
      loaded_successfully_ = true;
      return;
    }
    // Chop the last linefeed if present.
    int len = strlen(result);
    if (len > 0 && result[len - 1] == '\n') {  // Linefeed.
      result[--len] = '\0';
    }
    if (len > 0 && result[len - 1] == '\r') {  // Carriage return.
      result[--len] = '\0';
    }
    if (line_callback_) line_callback_(result);
  }
  data_file->Close();
}


}  // namespace PS

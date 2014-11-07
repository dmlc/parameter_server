#include "util/filelinereader.h"

#include <cstring>
#include <string>
#include <memory>

#include "util/file.h"
#include "glog/logging.h"
#include "util/common.h"

namespace PS {

DEFINE_int32(line_limit, 0,
  "line number limit that one data file could read");

void FileLineReader::Reload() {
  const int kMaxLineLength = 60 * 1024;
  File* const data_file = File::open(data_conf_, "r");
  if (data_file == NULL) {
    loaded_successfully_ = false;
    return;
  }

  size_t readed_line_count = 0;
  std::unique_ptr<char[]> line(new char[kMaxLineLength]);
  for (;;) {
    char* const result = data_file->readLine(line.get(), kMaxLineLength);
    if (result == NULL ||
        (FLAGS_line_limit > 0 && readed_line_count > FLAGS_line_limit)) {
      data_file->close();
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

    // increase line counter
    readed_line_count++;
  }
  data_file->close();
}


}  // namespace PS

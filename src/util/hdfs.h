#pragma once

#include "util/common.h"
#include "proto/config.pb.h"

namespace PS {

// TODO read home from $HDFS_HOME if empty
static string hadoopFS(const HDFSConfig& conf) {
  return (conf.home() + "/bin/hadoop dfs -D fs.default.name=" + conf.namenode()
          + " -D hadoop.job.ugi=" + conf.ugi());
}

} // namespace PS

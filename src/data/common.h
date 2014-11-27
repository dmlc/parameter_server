#pragma once
#include "util/common.h"
#include "proto/config.pb.h"
#include "proto/instance.pb.h"
#include "proto/example.pb.h"
#include "proto/matrix.pb.h"

namespace PS {

DECLARE_string(input);
DECLARE_string(output);
DECLARE_string(format);

// return files matches the regex in *config*
DataConfig searchFiles(const DataConfig& config);

// evenly parttion the files into *num* parts
std::vector<DataConfig> divideFiles(const DataConfig& data, int num);

// locate the i-th file in *conf*, append it with suffix, and keep the rest metadata
DataConfig ithFile(const DataConfig& conf, int i, const string& suffix = "");

InstanceInfo mergeInstanceInfo(const InstanceInfo& A, const InstanceInfo& B);

ExampleInfo mergeExampleInfo(const ExampleInfo& A, const ExampleInfo& B);

MatrixInfo readMatrixInfo(
    const ExampleInfo& info, int slot_id, int sizeof_idx, int sizeof_val);



} // namespace PS

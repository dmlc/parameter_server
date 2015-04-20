#pragma once
#include "util/common.h"
#include "data/proto/example.pb.h"
#include "data/proto/data.pb.h"
#include "util/proto/matrix.pb.h"

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

// return A + B
DataConfig appendFiles(const DataConfig& A, const DataConfig& B);

ExampleInfo mergeExampleInfo(const ExampleInfo& A, const ExampleInfo& B);

MatrixInfo readMatrixInfo(
    const ExampleInfo& info, int slot_id, int sizeof_idx, int sizeof_val);


DataConfig shuffleFiles(const DataConfig& data);


} // namespace PS

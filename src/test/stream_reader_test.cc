#include "gtest/gtest.h"
#include "data/stream_reader.h"
#include "base/matrix_io_inl.h"
#include "base/localizer.h"

using namespace PS;

// TEST(StreamReader, read_proto) {
//   DataConfig dc;
//   // load adfea
//   dc.set_format(DataConfig::PROTO);
//   dc.add_file("../output/parsa_.*");

//   DataConfig dc2 = searchFiles(dc);
//   StreamReader<double> reader; reader.init(dc2);

//   MatrixPtrList<double> X;
//   while (reader.readMatrices(10000, &X)) {
//     CHECK_EQ(X.size(), 2);
//   }
// }

// TEST(StreamReader, read) {
//   DataConfig dc;
//   // load adfea
//   dc.set_format(DataConfig::TEXT);
//   dc.set_text(DataConfig::ADFEA);
//   dc.add_file("../../data/ctrc/train/part-000[0-1].gz");
//   dc.set_ignore_feature_group(true);

//   // load libsvm
//   // dc.set_text(DataConfig::LIBSVM);
//   // dc.add_file("../data/rcv1/train/part-.*");

//   DataConfig dc2 = searchFiles(dc);
//   StreamReader<double> reader; reader.init(dc2);

//   MatrixPtrList<double> X;
//   reader.readMatrices(100, &X);
// }


// TEST(StreamReader, convert) {
//   DataConfig dc;
//   dc.set_format(DataConfig::TEXT);
//   dc.set_text(DataConfig::TERAFEA);
//   dc.add_file("../data/toutiao/data.txt");
//   dc.set_ignore_feature_group(true);

//   DataConfig dc2 = searchFiles(dc);
//   StreamReader<double> reader; reader.init(dc2);

//   MatrixPtrList<double> X;
//   reader.readMatrices(1000000, &X);
//   // CHECK_EQ(X.size(), 2);

//   X[0]->writeToBinFile("toutiao.Y");

//   for (int i = 1; i < X.size(); ++i) {
//     Localizer<Key, double> localizer;
//     SArray<Key> key;
//     localizer.countUniqIndex(X[i], &key);
//     key.writeToFile("toutiao.key."+std::to_string(X[i]->info().id()));
//     auto Z = localizer.remapIndex(key);
//     Z->writeToBinFile("toutiao.X."+std::to_string(X[i]->info().id()));
//   }
// }

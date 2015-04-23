#pragma once
#include "system/customer.h"
#include "data/stream_reader.h"
#include "util/evaluation.h"
namespace PS {

#if USE_S3
bool s3file(const std::string& name);
std::string s3Prefix(const std::string& path);
std::string s3Bucket(const std::string& path);
std::string s3FileUrl(const std::string& path);
#endif // USE_S3


namespace LM {

class ModelEvaluation : public App {
 public:
  ModelEvaluation(const Config& conf) : App(), conf_(conf) { }
  virtual ~ModelEvaluation() { }
  virtual void Run();
 private:
  typedef float Real;
  Config conf_;
};

void ModelEvaluation::Run() {
  if (!IsScheduler()) return;
  // load model
  std::unordered_map<Key, Real> weight;
  auto model = searchFiles(conf_.model_input());
  NOTICE("find %d model files", model.file_size());
  for (int i = 0; i < model.file_size(); ++i) {
#if USE_S3
    std::ifstream in;
    if (s3file(model.file(i))) {
      // download file from s3
      std::string cmd="curl -s -o model_file "+s3FileUrl(model.file(i));
      LOG(INFO)<<cmd;
      system(cmd.c_str());
      in.open("model_file"); 
    }
    else {
      in.open(model.file(i));
    }
#else
    std::ifstream in(model.file(i));
#endif // USE_S3
    while (in.good()) {
      Key k; Real v;
      in >> k >> v;
      weight[k] = v;
    }
  }
#if USE_S3
 // remove local model after read done
 std::string cmd="rm -rf model_file";
 system(cmd.c_str());
#endif // USE_S3

  NOTICE("load %lu model entries", weight.size());

  // load evaluation data and compute the predicted value
  auto data = searchFiles(conf_.validation_data());
  data.set_ignore_feature_group(true);
  NOTICE("find %d data files", data.file_size());

  SArray<Real> label;
  SArray<Real> predict;
  MatrixPtrList<Real> mat;
  StreamReader<Real> reader(data);
  // TODO read in an another thread
  bool good = false;
  do {
    good = reader.readMatrices(100000, &mat);
    CHECK_EQ(mat.size(), 2);
    label.append(mat[0]->value());

    SArray<Real> Xw(mat[1]->rows()); Xw.SetZero();
    auto X = std::static_pointer_cast<SparseMatrix<Key, Real>>(mat[1]);
    for (int i = 0; i < X->rows(); ++i) {
      Real re = 0;
      for (size_t j = X->offset()[i]; j < X->offset()[i+1]; ++j) {
        // TODO build a bloom filter
        auto it = weight.find(X->index()[j]);
        if (it != weight.end()) {
          re += it->second * (X->binary() ? 1 : X->value()[j]);
        }
      }
      Xw[i] = re;
    }
    predict.append(Xw);
    printf("\r                                             \r");
    printf("  load %lu examples", label.size());
    fflush(stdout);
  } while (good);
  printf("\n");

  // label.writeToFile("label");
  // predict.writeToFile("predict");

  // evaluation

  NOTICE("auc: %f", Evaluation<Real>::auc(label, predict));
  NOTICE("accuracy: %f", Evaluation<Real>::accuracy(label, predict));
  NOTICE("logloss: %f", Evaluation<Real>::logloss(label, predict));
}

} // namespace LM
} // namespace PS

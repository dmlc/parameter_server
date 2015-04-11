#pragma once
#include "system/customer.h"
#include "data/stream_reader.h"
#include "util/evaluation.h"
namespace PS {
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
    std::ifstream in(model.file(i));
    while (in.good()) {
      Key k; Real v;
      in >> k >> v;
      weight[k] = v;
    }
  }

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
    NOTICE("load %lu examples", label.size());
  } while (good);

  // label.writeToFile("label");
  // predict.writeToFile("predict");

  // evaluation
  NOTICE("auc: %f", Evaluation<Real>::auc(label, predict));
  NOTICE("accuracy: %f", Evaluation<Real>::accuracy(label, predict));
}

} // namespace LM
} // namespace PS

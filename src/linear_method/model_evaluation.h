#pragma once
#include "linear_method/linear_method.h"
#include "base/evaluation.h"
#include "data/stream_reader.h"
namespace PS {
namespace LM {

class ModelEvaluation : public LinearMethod {
 public:
  void run();
  virtual void process(const MessagePtr& msg) { }

 private:
  typedef float Real;
};

void ModelEvaluation::run() {
  // load model
  std::unordered_map<Key, Real> weight;
  auto model = searchFiles(conf_.model_input());
  LI << "find " << model.file_size() << " model files";
  for (int i = 0; i < model.file_size(); ++i) {
    std::ifstream in(model.file(i));
    while (in.good()) {
      Key k; Real v;
      in >> k >> v;
      weight[k] = v;
    }
  }
  LI << "load " << weight.size() << " model entries";

  // load evaluation data and compute the predicted value
  auto data = searchFiles(conf_.validation_data());
  data.set_ignore_feature_group(true);
  LI << "find " << data.file_size() << " data files";

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

    SArray<Real> Xw(mat[1]->rows()); Xw.setZero();
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
    LI << "load " << label.size() << " examples";
  } while (good);

  // evaluation
  // label.writeToFile("L");
  // predict.writeToFile("P");
  LI << "auc: " << Evaluation<Real>::auc(label, predict);
}

} // namespace LM
} // namespace PS

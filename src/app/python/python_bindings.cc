#include <Python.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include <boost/python.hpp>
#include <boost/numpy.hpp>
#pragma GCC diagnostic pop
#include <mutex>
#include "ps.h"
#include "python_bindings.h"
#include "shared_model.h"

namespace PS {

static SharedModel<float> *g_shared_model = nullptr;

static void prepare_shared_model() {
  static std::mutex mu;

  if (!g_shared_model) {
    std::lock_guard<std::mutex> lg(mu);
    if (!g_shared_model)
      g_shared_model = new SharedModel<float>();
  }
}

static std::size_t ndarray_num_elements(const boost::numpy::ndarray& arr) {
  std::size_t size = 1;
  for (int i = 0; i < arr.get_nd(); i++)
    size *= arr.shape(i);
  return size;
}

static void PullWeight(boost::numpy::ndarray weight, std::string name) {
  prepare_shared_model();

  std::size_t size = ndarray_num_elements(weight);
  if (size == 0)
    return;

  // pull
  int push_time = -1;
  float* weight_data = reinterpret_cast<float*>(weight.get_data());
  g_shared_model->setLayer(name, weight_data, size);    // in-place write to ndarray
  MessagePtr pull_msg(new Message(kServerGroup, -1, push_time));
  pull_msg->task.set_key_channel_str(name);
  Range<Key>(0, size).to(pull_msg->task.mutable_key_range());
  pull_msg->wait = true;
  g_shared_model->pull(pull_msg);
}

static void PushGradAndPullWeight(boost::numpy::ndarray grad, boost::numpy::ndarray weight, std::string name) {
  prepare_shared_model();

  std::size_t size = ndarray_num_elements(weight);
  if (size == 0)
    return;

  assert(size == ndarray_num_elements(grad));

  // push
  float* grad_data = reinterpret_cast<float*>(grad.get_data());
  SArray<float> val(grad_data, size, false);    // zero-copy read from ndarray
  MessagePtr push_msg(new Message(kServerGroup));
  push_msg->addValue(val);
  // LL << val;
  push_msg->task.set_key_channel_str(name);
  Range<Key>(0, size).to(push_msg->task.mutable_key_range());
  int push_time = CHECK_NOTNULL(g_shared_model)->push(push_msg);

  // pull
  float* weight_data = reinterpret_cast<float*>(weight.get_data());
  g_shared_model->setLayer(name, weight_data, size);    // in-place write to ndarray
  MessagePtr pull_msg(new Message(kServerGroup, -1, push_time));
  pull_msg->task.set_key_channel_str(name);
  Range<Key>(0, size).to(pull_msg->task.mutable_key_range());
  pull_msg->wait = true;
  g_shared_model->pull(pull_msg);
}

} // namespace PS

BOOST_PYTHON_MODULE(_ps) {
  using namespace boost::python;
  using namespace PS;

  def("myNodeID", MyNodeID);
  def("myRank", MyRank);
  def("rankSize", RankSize);

  def("PullWeight", PullWeight);
  def("PushGradAndPullWeight", PushGradAndPullWeight);
}

namespace PS {

void init_bindings() {
  init_ps();
}

} // namespace PS


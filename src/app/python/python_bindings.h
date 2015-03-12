#pragma once
#include <Python.h>
#include <boost/python.hpp>
#include <boost/numpy.hpp>
#include <mutex>
#include "ps.h"
#include "shared_model.h"

namespace PS {

static SharedModel<float> *shared_model = nullptr;

static void PushGradAndPullWeight(boost::numpy::ndarray grad, boost::numpy::ndarray weight, std::string name) {
  static std::mutex mu;

  if (!shared_model) {
    std::lock_guard<std::mutex> lg(mu);
    if (!shared_model)
      shared_model = new SharedModel<float>();
  }

  std::size_t size = 1;
  for (int i = 0; i < weight.get_nd(); i++)
    size *= weight.shape(i);
  if (size == 0)
    return;

  // push
  float* grad_data = reinterpret_cast<float*>(grad.get_data());
  SArray<float> val; val.copyFrom(grad_data, size);
  MessagePtr push_msg(new Message(kServerGroup));
  push_msg->addValue(val);
  // LL << val;
  push_msg->task.set_key_channel_str(name);
  Range<Key>(0, size).to(push_msg->task.mutable_key_range());
  int push_time = CHECK_NOTNULL(shared_model)->push(push_msg);

  // pull
  float* weight_data = reinterpret_cast<float*>(weight.get_data());
  shared_model->setLayer(name, weight_data, size);
  MessagePtr pull_msg(new Message(kServerGroup, -1, push_time));
  pull_msg->task.set_key_channel_str(name);
  Range<Key>(0, size).to(pull_msg->task.mutable_key_range());
  pull_msg->wait = true;
  shared_model->pull(pull_msg);
}

static void PullWeight(boost::numpy::ndarray weight, std::string name) {
  static std::mutex mu;

  if (!shared_model) {
    std::lock_guard<std::mutex> lg(mu);
    if (!shared_model)
      shared_model = new SharedModel<float>();
  }

  std::size_t size = 1;
  for (int i = 0; i < weight.get_nd(); i++)
    size *= weight.shape(i);
  if (size == 0)
    return;

  // pull
  int push_time = -1;
  float* weight_data = reinterpret_cast<float*>(weight.get_data());
  shared_model->setLayer(name, weight_data, size);
  MessagePtr pull_msg(new Message(kServerGroup, -1, push_time));
  pull_msg->task.set_key_channel_str(name);
  Range<Key>(0, size).to(pull_msg->task.mutable_key_range());
  pull_msg->wait = true;
  shared_model->pull(pull_msg);
}

} // namespace PS


BOOST_PYTHON_MODULE(ps) {
  using namespace boost::python;
  using namespace PS;

  def("myNodeID", MyNodeID);
  def("myRank", MyRank);
  def("rankSize", RankSize);

  def("PullWeight", PullWeight);
  def("PushGradAndPullWeight", PushGradAndPullWeight);
}


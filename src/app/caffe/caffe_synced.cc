#include <iostream>
#include <chrono>
#include <thread>
#include <google/protobuf/io/coded_stream.h>

#include "ps.h"
#include "system/app.h"
#include "parameter/v_vector.h"
#include "parameter/kv_vector.h"
#include "caffe/caffe.hpp"
#include "caffe/util/math_functions.hpp"
using caffe::Blob;
using caffe::Solver;
using caffe::SolverParameter;
using caffe::Caffe;
using caffe::caffe_scal;
using google::protobuf::io::CodedInputStream;

// caffe cmd flags

DEFINE_int32(gpu, -1,
    "Run in GPU mode on given device ID.");
DEFINE_string(solver, "",
    "The solver definition protocol buffer text file.");
DEFINE_string(model, "",
    "The model definition protocol buffer text file..");
DEFINE_string(snapshot, "",
    "Optional; the snapshot solver state to resume training.");


// client puller / pusher flags
DEFINE_int32(pushstep, 3,
    "interval, in minibatches, between push operation.");
DEFINE_int32(pullstep, 3,
    "DEPRECATED interval, in minibatches, between pull operation.");



caffe::SolverParameter solver_param;
Solver<float>* initCaffeSolver(){

  Solver<float>* solver;

  CHECK_GT(FLAGS_solver.size(), 0) << "Need a solver definition to train.";

  caffe::ReadProtoFromTextFileOrDie(FLAGS_solver, &solver_param);

  if (FLAGS_gpu < 0
      && solver_param.solver_mode() == caffe::SolverParameter_SolverMode_GPU) {
    FLAGS_gpu = solver_param.device_id();
  }

  // Set device id and mode
  if (FLAGS_gpu >= 0) {
    LOG(INFO) << "Use GPU with device ID " << FLAGS_gpu;
    Caffe::SetDevice(FLAGS_gpu);
    Caffe::set_mode(Caffe::GPU);
  } else {
    LOG(INFO) << "Use CPU.";
    Caffe::set_mode(Caffe::CPU);
  }

  solver = caffe::GetSolver<float>(solver_param);

  if (FLAGS_snapshot.size()) {
    LOG(INFO) << "Resuming from " << FLAGS_snapshot;
    solver->Restore(FLAGS_snapshot.c_str());
  }

  return solver;
}

caffe::Net<float>* initCaffeNet(){
  CHECK_GT(FLAGS_solver.size(), 0) << "Need a solver definition to train.";

  caffe::ReadProtoFromTextFileOrDie(FLAGS_solver, &solver_param);

  std::string net_path = solver_param.net();

  return NULL;// todo
}

#define V_WEIGHT "weight"
#define V_DIFF "diff"
#define V_SOLVER "solver"
namespace PS {
class CaffeServer : public App, public VVListener<float>, public VVListener<char> {
 public:
  CaffeServer(const string& name, const string& conf) : App(name) { }
  virtual ~CaffeServer() { }

  virtual void init() {
    LL << myNodeID() << ", this is server " << myRank();

    solver = initCaffeSolver();

    // initialize the weight at server
    int total_weight = 0;
    weights = new VVector<float>(V_WEIGHT, true, this);
    diffs = new VVector<float>(V_DIFF, false, this);
    solver_states = new VVector<char>(V_SOLVER, true, this);

    for (int i = 0; i < solver->net()->params().size();i++){
      auto blob = solver->net()->params()[i];
      weights->value(i).reset(blob->mutable_cpu_data(), blob->count(), false);
      Blob<float>* newBlob = new Blob<float>(blob->num(), blob->channels(), blob->height(), blob->width());
      diffBlobs.push_back(newBlob);
      diffs->value(i).reset(newBlob->mutable_cpu_diff(), newBlob->count(), false);
      Blob<float>* accBlob = new Blob<float>(blob->num(), blob->channels(), blob->height(), blob->width());
      accDiffBlobs.push_back(accBlob);
      total_weight += blob->data()->size();
    }

    LL << "total weight size:" << total_weight;
  }
  
  void testPhase(){
    Lock l(mu_solver);
    solver->TestAll();
  }

  void snapshotPhase(){
    Lock l(mu_solver);
    solver->Snapshot();
  }

  void resetDiffCount() {
    Lock l(mu_accDiff);
    accDiffCount = 0;
    for(auto acc : diffBlobs){
      memset(acc->mutable_cpu_diff(), 0, acc->diff()->size());
    }
    for(auto acc : accDiffBlobs){
      memset(acc->mutable_cpu_diff(), 0, acc->diff()->size());
    }
  }

  void signalWorkersStart(){
    resetDiffCount();
    Task task;
    auto sgd = task.mutable_sgd();
    sgd->set_cmd(SGDCall::UPDATE_MODEL);
    port(kWorkerGroup)->submitAndWait(task);
//    port(kWorkerGroup)->submit(task);
    LL << "signalWorkerStart returned";
  }

  void gatherDiffs(){
    //TODO do not need to synchronize all workers?
    /*
    while(true){
      {
        Lock l0(mu_accDiff);
        if(accDiffCount == sys_.yp().num_workers()){
          break;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    */
//    LL << "all diff gathered: " << accDiffCount;
    Lock l(mu_solver);
    Lock l2(mu_accDiff);
    CHECK_EQ(accDiffCount, sys_.yp().num_workers()) << "accDiffCount should be same as num_workers!";
//    float first,last, firstv, lastv;
    for (int i = 0; i < solver->net()->params().size();i++){
      auto blob = solver->net()->params()[i];
      float* dest = blob->mutable_cpu_diff();
      auto src = accDiffBlobs[i];
      memcpy(dest, src->cpu_diff(), blob->diff()->size());

/*      if(i==0){
    first=blob->cpu_diff()[0];
    firstv = src[0];
      }else if(i == solver->net()->params().size()-1){
    last=blob->cpu_diff()[blob->count()-1];
    lastv = src[src.size() - 1];
      }
*/
    }
    LL << "accDiffs copied to net->params->diff";

  }


  void run() {
    LL << myNodeID() << ", server " << myRank() << " run()ing";
    auto param = solver->param();
    int iter = solver->param().max_iter() - solver->iter();
    LL << "start training loop";
    for (int i = 0; i < iter; i++) {
      signalWorkersStart();
      gatherDiffs();
      solver->ComputeUpdateValue();
      solver->net()->Update();
      solver->snapshotPhase();
      solver->stepEnd();
    }
  }

  void process(const MessagePtr& msg) {
    auto sgd = msg->task.sgd();
    if (sgd.cmd() == SGDCall::UPDATE_MODEL) { // sync param to memory
    {
      Lock l(mu_solver);
    }
    }
  }

  void vectorChanged(VVector<float>* data){
//    LL << "vector change received:" << data->name();
    CHECK_EQ(data, this->diffs) << "server only accept diff changes";

    Lock l(mu_accDiff);
    // append diffs to accDiffs
//    float first,last, firstv, lastv;
    for (int i = 0; i < diffBlobs.size();i++) {
      auto blob = diffBlobs[i];
      float* dest = accDiffBlobs[i]->mutable_cpu_diff();
      float* src = diffBlobs[i]->mutable_cpu_diff();
      //scale down?
      if(FLAGS_pushstep != 0){
        caffe::caffe_scal(blob->count(), float(1.0 / FLAGS_pushstep), src);
      }

      caffe::caffe_add(blob->count(), src, dest, dest);

/*      if(i==0){
	  first=blob->cpu_diff()[0];
	  firstv = src[0];
      }else if(i == solver->net()->params().size()-1){
	  last=blob->cpu_diff()[blob->count()-1];
	  lastv = src[src.size() - 1];
      }
*/
    }
    accDiffCount++;
//    LL << "diff gathered: # " << accDiffCount;
//    LL<< "got diff[" << first<<",...,"<<last<<"]/[" << firstv << ",...," << lastv <<"]";

 }
  void vectorChanged(VVector<char>* data){
    CHECK(false) << "shouldn't be any VVector<char> change: "<< data->name();
  }

  void vectorGetting(VVector<float>* data){
    Lock l(mu_solver);
    float first, last;
    if (data == this->weights){
      // need to sync to CPU
      for (int i = 0; i < solver->net()->params().size();i++){
        auto blob = solver->net()->params()[i];
        blob->cpu_data();
        if (i==0) {
          first = blob->cpu_data()[0];
        } else if (i == solver->net()->params().size() - 1 ) {
          last = blob->cpu_data()[blob->count()-1];
        }
      }
      LL << "weight synced: ["<<first<<","<<last<<"]";
    } else {
      CHECK(false) << "some one is getting none-gettable! " << data->name();
    }
  }

  void vectorGetting(VVector<char>* data){
    LL << "getting char: "<< data->name();
    Lock l(mu_solver);
    if (data == this->solver_states){
      // need to serialize solver state into solver_states
      caffe::SolverState state;
      this->solver->SnapshotSolverState(&state);
      state.set_iter(solver->iter());
      state.set_current_step(solver->current_step());
      string buf;
      state.SerializeToString(&buf);
      solver_states->value(0).resize( buf.size() );
      memcpy(solver_states->value(0).data(), buf.data(), buf.size());
      LL << "server solver state saved, history:" << state.history_size() << ", total:" << buf.size();
    } else {
      CHECK(false) << "some one is getting none-gettable! " << data->name();
    }
  }


 private:
  VVector<char> *solver_states; // individual data ptr, solver state to initialize workers
  VVector<float> *weights; //share data ptr with solver->net->params->cpu_data
  VVector<float> *diffs; //individual data ptr with diffBlobs
  std::vector<Blob<float>*> diffBlobs;

  std::vector<Blob<float>*> accDiffBlobs;
  int accDiffCount;
  std::mutex mu_accDiff; // guard accDiffCount and accDiffBlobs

  std::mutex mu_solver;
  caffe::Solver<float>* solver;
};

App* CreateServerNode(const std::string& conf) {
  return new CaffeServer("app", conf);
}


class CaffeWorker: public App{
private:

  std::mutex mu_weight; // protect write to weight_ready and weights
  volatile bool weight_ready;
  std::mutex mu_diff;  //protect write to diffs

  VVector<char> *solver_states; // individual data ptr, solver state to initialize workers

  VVector<float> *weights;// individual data ptr, same order/size as solver->net->params
  VVector<float> *diffs;// for accumulated diff, share memory with diffBlobs
  std::vector<Blob<float>*> diffBlobs;
  caffe::Solver<float>* solver;

  volatile unsigned int tickDiff=0, tickStep=0;

  volatile bool _terminate = false;

public:
  CaffeWorker(const string& name, const string& conf):App(name){

  }
  ~CaffeWorker(){

  }

  void init(){
    LL << "worker init()";
    weight_ready = false;
    solver = initCaffeSolver();
    //init shared parameter at worker
    weights = new VVector<float>(V_WEIGHT);
    diffs = new VVector<float>(V_DIFF);
    solver_states = new VVector<char>(V_SOLVER);
    solver_states->value(0) = {};
    solver_states->setResizable(true);

    for (int i = 0; i < solver->net()->params().size();i++){
      auto blob = solver->net()->params()[i];
      weights->value(i).resize(blob->count());
      Blob<float>* newBlob = new Blob<float>(blob->num(), blob->channels(), blob->height(), blob->width());
      diffBlobs.push_back(newBlob);
      diffs->value(i).reset(newBlob->mutable_cpu_diff(), newBlob->count(), false);
    }
    LL << "worker init() over";
  }

  /**
   * by main
   */
  void run(){
    LL << "worker run()";
    LL << "worker run() over";
  }

  void process(const MessagePtr& msg) {
    LL << "message received";
    auto sgd = msg->task.sgd();
    if (sgd.cmd() == SGDCall::UPDATE_MODEL) { // sync param to memory
      LL << "update model received";
      pullWeight();
      swapWeight();
      LL << "weight got";
      for (int i = 0; i < FLAGS_pushstep; i++) {
        solver->testPhase();
        solver->forwardBackwardPhase();
        this->accumulateDiff();
        solver->displayPhase();
        // bypass all of computeUpdateValue
        solver->stepEnd();
        stepEnd();
      }
      LL << "pushstep " << FLAGS_pushstep << "reached.";
      pushDiff();
    }
  }

  /**
   * notify accumulateDiff end, for pusher counting
   */
  void diffEnd(){
    tickDiff++;
  }
  /**
   * for puller counting
   */
  void stepEnd(){
    tickStep++;
  }

  /**
   * by pusher, synchronized (block until message sent)
   */
  void pushDiff(){
    Lock l(mu_diff);
    //push to app instead of
    MessagePtr msg(new Message(kServerGroup));
    msg->key = {0};
    msg->task.set_key_channel(0);
    for(int i = 0; i < diffs->vcount();i++){
      auto acc = diffBlobs[i];
      acc->cpu_diff(); // sync to cpu
      auto diff = diffs->value(i);
      CHECK_EQ(acc->cpu_diff(), diff.data());
      msg->addValue(diff);
    }
    int push_time = diffs->push(msg);
    diffs->waitOutMsg(kServerGroup, push_time);
    //clear previous diff
    for(auto acc : diffBlobs){
      memset(acc->mutable_cpu_diff(), 0, acc->diff()->size());
    }
  }

  /**
   * by main
   */
  void accumulateDiff(){
    {
      Lock l(mu_diff);
      for(int i = 0; i < solver->net()->params().size(); i++){
        auto acc = diffBlobs[i];
        auto blob = solver->net()->params()[i];
        switch (Caffe::mode()) {
          case Caffe::CPU:
            caffe::caffe_add(acc->count(), blob->cpu_diff(), acc->cpu_diff(), acc->mutable_cpu_diff());
            break;
          case Caffe::GPU:
            caffe::caffe_gpu_add(acc->count(), blob->gpu_diff(), acc->gpu_diff(), acc->mutable_gpu_diff());
            break;
          default:
            LOG(FATAL) << "Unknown caffe mode: " << Caffe::mode();
        }
      }
    }
    diffEnd();
  }

  /**
   * by main
   */
  void pullSolverState() {
    MessagePtr msg(new Message(kServerGroup));
    msg->key = {0};
    int pull_time = solver_states->pull(msg);
    solver_states->waitOutMsg(kServerGroup, pull_time);
    Lock l(mu_weight);
    SArray<char>src = solver_states->value(0);
    LL << "solver state got: " << src.size();
    caffe::SolverState state;
    CodedInputStream cis((const uint8*) src.data(), src.size());
    cis.SetTotalBytesLimit(INT_MAX, 536870912);
    state.ParseFromCodedStream(&cis);

    solver->RestoreSolverState(state);
    solver->setIter(state.iter());
    solver->setCurrentStep(state.current_step());
  }

  /**
   * by puller (except the initial pull), synchronized
   */
  void pullWeight(){
    LL << "begin pull weight";
//    Task task;
//    task.mutable_sgd()->set_cmd(SGDCall::UPDATE_MODEL);
//    port(kServerGroup)->submitAndWait(task);

    Lock l(mu_weight);
    if(weight_ready){
      LL << "weight_ready!";
      return;
    }
    MessagePtr msg(new Message(kServerGroup));
    msg->key = {0};
    LL << "begin pull";
    int pull_time = weights->pull(msg);
    LL << "begin waitOutMsg";
    weights->waitOutMsg(kServerGroup, pull_time);
    weight_ready = true;
    LL << "weight pulled from server, total:" << weights->totalSize();
  }
  /**
   * by main, copy received weight into solver->net
   */
  void swapWeight(){
    Lock l(mu_weight);
    if(!weight_ready){
      return;
    }
    float first,last;
    for (int i = 0; i < solver->net()->params().size();i++){
      auto blob = solver->net()->params()[i];
      float* dest = blob->mutable_cpu_data();
      auto src = weights->value(i);
      memcpy(dest, src.data(), blob->data()->size());
      //TODO direct copy to GPU?
      if(i == 0){
        first = blob->cpu_data()[0];
      }else if(i == solver->net()->params().size()-1){
        last = blob->cpu_data()[blob->count()-1];
      }
    }
    LL << "weight from server:[" << first << ",...," << last << "]";
    weight_ready = false;
  }
};
} // namespace PS

namespace PS {
App* App::create(const string& name, const string& conf) {
  auto my_role = Postoffice::instance().myNode().role();
  if (my_role == Node::SERVER) {
    return new CaffeServer(name, conf);
  } else if(my_role == Node::WORKER){
      return new CaffeWorker(name, conf);
  }else{
    return new App();
  }
}
} // namespace PS


int main(int argc, char *argv[]) {

  google::ParseCommandLineFlags(&argc, &argv, true);

  auto& sys = PS::Postoffice::instance();
  sys.start(&argc, &argv);

  sys.stop();
  return 0;
}


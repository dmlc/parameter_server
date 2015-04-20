#include <iostream>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <google/protobuf/io/coded_stream.h>

#include "ps.h"
#include "system/app.h"
#include "parameter/v_vector.h"
#include "parameter/kv_vector.h"
#include "caffe/caffe.hpp"
#include "caffe/common.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/util/upgrade_proto.hpp"
using namespace caffe;
using namespace std;
using caffe::Blob;
using caffe::Solver;
using caffe::SolverParameter;
using caffe::Caffe;
using caffe::caffe_scal;
using google::protobuf::io::CodedInputStream;
using std::string;
using std::vector;

// caffe cmd flags

DEFINE_int32(gpu, -1,
    "Run in GPU mode on given device ID.");
DEFINE_string(solver, "",
    "The solver definition protocol buffer text file.");
DEFINE_string(model, "",
    "The model definition protocol buffer text file..");
DEFINE_string(snapshot, "",
    "Optional; the snapshot solver state to resume training.");
DEFINE_string(workers, "W0",
    "cwd if workers, subdirectory of current directory.");
DEFINE_bool(fb_only, true,
    "DEPRECATED; workers only ForwardBackward.");
DEFINE_bool(synced, false,
    "DEPRECATED; pull/push synced with Forward");
// client puller / pusher flags
DEFINE_int32(pushstep, 3,
    "interval, in minibatches, between push operation.");
DEFINE_int32(pullstep, 3,
    "DEPRECATED interval, in minibatches, between pull operation.");

caffe::SolverParameter solver_param;
Solver<float>* initCaffeSolver(int id){

  Solver<float>* solver;

  CHECK_GT(FLAGS_solver.size(), 0) << "Need a solver definition to train.";

  caffe::ReadProtoFromTextFileOrDie(FLAGS_solver, &solver_param);

  if (id < 0) {
    id = FLAGS_gpu;
  }

  if (id < 0
      && solver_param.solver_mode() == caffe::SolverParameter_SolverMode_GPU) {
    id = solver_param.device_id();
  }

  // Set device id and mode
  if (id >= 0) {
    LOG(INFO) << "Use GPU with device ID " << id;
    Caffe::SetDevice(id);
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

  caffe::NetParameter net_param;
  std::string net_path = solver_param.net();
  caffe::ReadNetParamsFromTextFileOrDie(net_path, &net_param);
  return new caffe::Net<float>(net_param);
}

#define V_WEIGHT "weight"
#define V_DIFF "diff"
#define V_SOLVER "solver"
namespace PS {

static std::mutex mu_pwd;
Solver<float>* initCaffeSolverInDir(int id, string root){
  Lock l(mu_pwd);
  char* cwd = getcwd(nullptr,1024);
  LL << "cwd: " << cwd;
  CHECK(cwd != nullptr);
  CHECK(0 == chdir(root.c_str()));
  Solver<float>* solver = initCaffeSolver(id);
  CHECK(0 == chdir(cwd));
  free(cwd);
  return solver;
}

class CaffeServer : public App, public VVListener<float>, public VVListener<char> {
 public:
  CaffeServer(const string& name, const string& conf) : App(name) { }
  virtual ~CaffeServer() { }

  virtual void init() {
    LL << myNodeID() << ", this is server " << myRank();

    solver = initCaffeSolver(-1);

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
    float first,last;
    for (int i = 0; i < diffBlobs.size();i++) {
      auto blob = diffBlobs[i];
      float* dest = accDiffBlobs[i]->mutable_cpu_diff();
      float* src = diffBlobs[i]->mutable_cpu_diff();
      if(i==0){
        first=blob->cpu_diff()[0];
      }else if(i == solver->net()->params().size()-1){
        last=blob->cpu_diff()[blob->count()-1];
      }
      //check
      bool isNan = false;
      int nanIndex = -1;
      for (int j = 0; j < diffBlobs[i]->count(); j++){
        if(isnan(src[j])){
          isNan = true;
          nanIndex = j;
        }
      }
      if(isNan){
        LL << "NAN in diffBlob[" << i << "][" << nanIndex << "]!";
      }
      //scale down?
      if(FLAGS_pushstep != 0){
        caffe::caffe_scal(blob->count(), float(1.0 / FLAGS_pushstep), src);
      }

      caffe::caffe_add(blob->count(), src, dest, dest);

    }
    accDiffCount++;
//    LL << "diff gathered: # " << accDiffCount;
    LL<< "got diff[" << first<<",...,"<<last<<"]";

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


class CaffeWorker;

class NetForwarder {

  bool terminated;
  int id;
  CaffeWorker* worker;
  string rootDir;
  caffe::Solver<float>* solver;
  std::mutex mu_forward;
  std::condition_variable cv_forward;
  bool start_forward;
  std::unique_ptr<std::thread> internalThread;

  bool needDisplay;

public:
  NetForwarder(CaffeWorker* parent, int id, string workerRoot, bool display):
    id(id),worker(parent),rootDir(workerRoot),needDisplay(display){
  }

  /**
   * by CaffeForwarder
   */
  void waitForwardSignal(){
    std::unique_lock<std::mutex> l(mu_forward);
    while(!start_forward){
      cv_forward.wait(l);
    }
  }

  /**
   * by CaffeForwarder
   */
  void signalForwardEnd(){
    std::unique_lock<std::mutex> l(mu_forward);
    start_forward = false;
    cv_forward.notify_all();
  }

  /**
   * by CaffeWorker
   */
  void signalForward() {
    std::unique_lock<std::mutex> l(mu_forward);
    start_forward = true;
    cv_forward.notify_all();
  }

  /**
   * by CaffeWorker
   */
  void joinForwardEnd() {
    if(!start_forward){
      return;
    }
    {
      std::unique_lock<std::mutex> l(mu_forward);
      while(start_forward) {
        cv_forward.wait(l);
      }
    }
  }

  void copyWeight();

  void accumulateDiff();

  void start() {
    if(nullptr == solver) {
      solver = initCaffeSolverInDir(id, rootDir);
      LL << "Inited solver On device id # " << id;
    }
    /*
    if (this->id >= 0) {
      LOG(INFO) << "Net Use GPU with device ID " << this->id;
      Caffe::SetDevice(this->id);
      Caffe::set_mode(Caffe::GPU);
    } else {
      LOG(INFO) << "Use CPU.";
      Caffe::set_mode(Caffe::CPU);
    }
    */
//    this->net = initCaffeNet();
    std::vector<Blob<float>*> bottom_vec;
    while(true) {
      // wait signal to forward
      waitForwardSignal();
      LL << "run() forward signal received";
      copyWeight();
      for (int i = 0; i < FLAGS_pushstep; i++) {
        if(needDisplay){
          solver->testPhase();
        }
        LL<< "forwarder # " << id;
        solver->forwardBackwardPhase();
        this->accumulateDiff();
        if(needDisplay){
          solver->displayPhase();
        }
        // bypass all of computeUpdateValue
        solver->stepEnd();
      }
      LL << "pushstep " << FLAGS_pushstep << " reached.";
      LL << "Forwarder sending forward end signal";
      signalForwardEnd();
    }
  }

  void startAsync(){
    if(!internalThread.get()){
      internalThread.reset(new thread(&NetForwarder::start, this));
    }
  }

  void stop() {
    //TODO
  }
};

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

class CaffeWorker: public App{
private:

  std::mutex mu_weight; // protect write to weight_ready and weights
  std::mutex mu_diff;  //protect write to diffs


  std::mutex mu_forward;
  std::condition_variable cv_forward;
  bool start_forward;

  VVector<float> *weights;// individual data ptr, same order/size as solver->net->params
  VVector<float> *diffs;// for accumulated diff, share memory with diffBlobs
  std::vector<Blob<float>*> diffBlobs;
  caffe::Solver<float>* solver;

  volatile bool _terminate = false;

  std::vector<NetForwarder*> forwarders;

public:
  CaffeWorker(const string& name, const string& conf):App(name){

  }
  ~CaffeWorker(){

  }

  void init(){
    LL << "worker init()";
    start_forward = false;
    solver = initCaffeSolver(-1);
    //init shared parameter at worker
    weights = new VVector<float>(V_WEIGHT);
    diffs = new VVector<float>(V_DIFF);

    for (int i = 0; i < solver->net()->params().size();i++){
      auto blob = solver->net()->params()[i];
      weights->value(i).resize(blob->count());
      Blob<float>* newBlob = new Blob<float>(blob->num(), blob->channels(), blob->height(), blob->width());
      diffBlobs.push_back(newBlob);
      diffs->value(i).reset(newBlob->mutable_cpu_diff(), newBlob->count(), false);
    }

    //init forwarders
    vector<string> workerRoots = split(FLAGS_workers, ',');
    char* cwd = getcwd(nullptr,1024);
    LL << "cwd: " << cwd;
    CHECK(cwd != nullptr);
    string cwdString(cwd);
    for (int id = 0; id < workerRoots.size(); id++){
      bool display = id == 0;
      string workerRoot = cwdString + "/" + workerRoots[id];
      LL << "creating forwarder in: " << workerRoot;
//      CHECK(0 == chdir(workerRoot.c_str()));
      NetForwarder* forwarder = new NetForwarder(this, id, workerRoot, display);
      forwarders.push_back(forwarder);
      forwarder->startAsync();
    }
//    CHECK(0 == chdir(cwd));
    free(cwd);
    LL << "worker init() over";
  }

  /**
   * by run() thread
   */
  void waitForwardSignal(){
    std::unique_lock<std::mutex> l(mu_forward);
    while(!start_forward){
      cv_forward.wait(l);
    }
  }

  /**
   * by run() thread
   */
  void signalForwardEnd(){
    std::unique_lock<std::mutex> l(mu_forward);
    start_forward = false;
    cv_forward.notify_all();
  }

  /**
   * by process() thread
   */
  void signalAndJoinForward() {
    std::unique_lock<std::mutex> l(mu_forward);
    start_forward = true;
    cv_forward.notify_all();
    while(start_forward) {
      cv_forward.wait(l);
    }
  }


  /**
   * by main
   */
  void run(){
    LL << "worker run()";
    while(true) {
      // wait signal to forward
      waitForwardSignal();
      LL << "run() forward signal received";
      pullWeight();
      for (int i = 0; i < forwarders.size(); i++){
        NetForwarder* forwarder = forwarders[i];
        forwarder->signalForward();
      }
      for (int i = 0; i < forwarders.size(); i++){
        NetForwarder* forwarder = forwarders[i];
        forwarder->joinForwardEnd();
      }
      LL << "all forwarder joined: " << forwarders.size();
      pushDiff();
      LL << "Worker sending forward end signal";
      signalForwardEnd();
    }
    LL << "worker run() over";
  }

  void process(const MessagePtr& msg) {
    LL << "message received";
    auto sgd = msg->task.sgd();
    if (sgd.cmd() == SGDCall::UPDATE_MODEL) { // sync param to memory
      LL << "process() update model received";
      signalAndJoinForward();
      LL << "process() forward end received";
    }
  }

  /**
   * by forwarder
   */
  void gatherDiff(Solver<float>* another) {
    Lock l(mu_diff);
    for(int i = 0; i < another->net()->params().size(); i++){
      auto acc = diffBlobs[i];
      auto blob = another->net()->params()[i];
      caffe::caffe_add(acc->count(), blob->cpu_diff(), acc->cpu_diff(), acc->mutable_cpu_diff());
    }
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
    float first, last;
    for(int i = 0; i < diffs->vcount();i++){
      auto acc = diffBlobs[i];
      acc->cpu_diff(); // sync to cpu
      auto diff = diffs->value(i);
      CHECK_EQ(acc->cpu_diff(), diff.data());
      msg->addValue(diff);
      if(i == 0){
        first = acc->cpu_diff()[0];
      }else if(i == diffs->vcount() - 1){
        last = acc->cpu_diff()[acc->count()-1];
      }
    }
    int push_time = diffs->push(msg);
    diffs->waitOutMsg(kServerGroup, push_time);
    LL << "Worker diff pushed:[" << first <<"," << last << "]";
    //clear previous diff
    for(auto acc : diffBlobs){
      memset(acc->mutable_cpu_diff(), 0, acc->diff()->size());
    }
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
    MessagePtr msg(new Message(kServerGroup));
    msg->key = {0};
    LL << "begin pull";
    int pull_time = weights->pull(msg);
    LL << "begin waitOutMsg";
    weights->waitOutMsg(kServerGroup, pull_time);
    LL << "weight pulled from server, total:" << weights->totalSize();
  }

  /**
   * by forwarder
   */
  void copyWeight(Solver<float>* another){
    float first,last;
    for (int i = 0; i < another->net()->params().size();i++){
      auto blob = another->net()->params()[i];
      float* dest = blob->mutable_cpu_data();
      auto src = weights->value(i);
      memcpy(dest, src.data(), blob->data()->size());
      //TODO direct copy to GPU?
      if(i == 0){
        first = blob->cpu_data()[0];
      }else if(i == another->net()->params().size()-1){
        last = blob->cpu_data()[blob->count()-1];
      }
    }
    LL << "weight from server:[" << first << ",...," << last << "]";
  }
};
void NetForwarder::copyWeight() {
  this->worker->copyWeight(this->solver);
}

void NetForwarder::accumulateDiff(){
  this->worker->gatherDiff(this->solver);
}

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


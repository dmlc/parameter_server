# Tutorial of the Parameter Server

This is a beginner guide for how to using the parameter server interface.

On the first [example](example_a.cc), we define worker nodes and server nodes by
=CreateServerNode= and =WorkerNodeMain=, respectively. Next ask workers to
push a list of key-value pairs into servers and then pull the new values back.

```c++
#include "ps.h"
typedef float Val;

int CreateServerNode(int argc, char *argv[]) {
  ps::KVServer<Val> server; server.Run();
  return 0;
}

int WorkerNodeMain(int argc, char *argv[]) {
  using namespace ps;
  std::vector<Key> key = {1, 3, 5};
  std::vector<Val> val = {1, 1, 1};
  std::vector<Val> recv_val(3);

  KVWorker<Val> wk;
  int ts = wk.Push(key, val);
  wk.Wait(ts);

  ts = wk.Pull(key, &recv_val);
  wk.Wait(ts);

  std::cout << MyNodeID() << ": " <<
      CBlob<Val>(recv_val).ShortDebugString() << std::endl;
  return 0;
}
```

This example can be compiled by =make -C .. guide= and run using 4 worker nodes
(processes) and 1 server node by =./local.sh 1 4 ./example_a=. A possible
output is
```
W2: [3]: 2 2 2
W0: [3]: 2 2 2
W1: [3]: 3 3 3
W3: [3]: 4 4 4
```

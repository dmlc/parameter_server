# Tutorial of the Parameter Server

This is a beginner guide for how to using the parameter server interface.

On the first example, we first define worker nodes and server nodes by
=CreateServerNode= and =WorkerNodeMain=, respectively. Then ask workers to
push a list of key-value pairs into servers, then pull the new values back

```c++
#include "ps.h"
typedef float Val;

int CreateServerNode(int argc, char *argv[]) {
  ps::KVServer<Val> server; server.Run();
  return 0;
}

int WorkerNodeMain(int argc, char *argv[]) {
  std::vector<Key> key = {1, 3, 5};
  std::vector<Val> val = {1, 1, 1};
  std::vector<Val> recv_val(3);

  ps::KVWorker<Val> worker;
  worker.Push(key, val);
  worker.Pull(key, val);

  std::cout << MyNodeID() << ": " <<
      CBlob<Val>(recv_val_1).ShortDebugString() << std::endl;
      return 0;
}
```

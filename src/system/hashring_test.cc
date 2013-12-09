#include "gtest/gtest.h"
#include "system/hashring.h"
#include "box/container.h"
#include "system/node.h"

using namespace PS;
using namespace std;
using namespace std;

TEST(HashRing, AddNode) {
  HashRing hashring(1);
  KeyRange key_range(1, 1000);
  hashring.SetKeyRange(key_range);
  
  hashring.AddCoolNode("0#master");
  hashring.AddNode("1#a", 9, 1);
  hashring.AddNode("2#b", 9, 2);
  hashring.AddNode("3#c", 9, 3);
  hashring.AddNode("4#d", 9, 4);
  hashring.AddNode("5#e", 9, 5);
  hashring.AddNode("6#f", 9, 6);
  hashring.AddNode("7#g", 9, 7);
  hashring.AddNode("8#h", 9, 8);

  std::map<Key, std::string> ring = hashring.GetRing();
  ASSERT_EQ(9, ring.size());
  ASSERT_EQ("0#master#0", ring.begin()->second);
  
  for(auto iter = ring.begin(); iter != ring.end(); iter++) {
    Key key = iter->first;
    string& value = iter->second;
    std::cout << key << "\t" << value << std::endl;
  }

}

class Container_stub : public Container {
  public:
  Container_stub(const string& name) : Container(name) {}
  Status GetLocalData(Mail *mail) {}
  Status MergeRemoteData(const Mail& mail) {}
};

TEST(HashRing, DivCollect) {
  HashRing hashring(1);
  KeyRange key_range(1, 1000);
  hashring.SetKeyRange(key_range);
  
  hashring.AddCoolNode("0#master");
  hashring.AddNode("1#a", 2, 1);
  //hashring.AddNode("2#b", 8, 1);
  //hashring.AddNode("3#c", 8, 2);
  //hashring.AddNode("4#d", 8, 3);
  //hashring.AddNode("5#e", 8, 4);
  //hashring.AddNode("6#f", 8, 5);
  //hashring.AddNode("7#g", 8, 6);
  //hashring.AddNode("8#h", 8, 7);

  Container_stub container("ctr");
  DHTInfo dhtinfo;

  std::map<Key, std::string> ring = hashring.GetRing();
  for(auto iter = ring.begin(); iter != ring.end(); iter++) {
    Key key = iter->first;
    string& value = iter->second;
    std::cout << key << "\t" << value << std::endl;
  }
  
  hashring.DivideKeyRange(&container, key_range, dhtinfo);
  hashring.CollectBkpInfo(&container, dhtinfo);

  std::cout << "Workloads: " << std::endl;
  for (auto iter = dhtinfo.workloads_.begin(); iter != dhtinfo.workloads_.end(); iter++) {
    std::cout << "(" << iter->first.first << ", " << iter->first.second << ")" << "\t" << iter->second.key_range().start()  << "," << iter->second.key_range().end() << std::endl;
  }

  std::cout << "bkp_nodes: " << std::endl;
  for (auto iter = dhtinfo.bkp_nodes_.begin(); iter != dhtinfo.bkp_nodes_.end(); iter++) {
    std::cout << "(" << iter->first.first << ", " << iter->first.second << ")" << "\t";
    std::vector<std::vector<PS::uid_t>> vec = iter->second;
    for (size_t i = 0; i < vec.size(); i++) {
      for (size_t j = 0; j < vec[i].size(); j++)
        cout << vec[i][j] << " ";
      cout << "|";
    }
    cout << endl;
  }

  std::cout << "bkp_workloads_:" << endl;
  for (auto iter = dhtinfo.bkp_workloads_.begin(); iter != dhtinfo.bkp_workloads_.end(); iter++) {
    cout << "(" << get<0>(iter->first) << ", " << get<1>(iter->first) << ", " << get<2>(iter->first) << ")\t";
    cout << iter->second.key_range().start() << ", " << iter->second.key_range().end() << endl;
  }
}

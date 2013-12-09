#include "system/hashring.h"
#include "util/common.h"
#include "box/container.h"

namespace PS {
  using std::vector;

  DEFINE_int32(num_backup, 1, "number of nodes that are used for backup one node");
  DEFINE_int32(num_replica, 1, "number of replicas");

  void HashRing::AddNode(const std::string& node_mark, const size_t n, const size_t i) {
    size_t num = (n - 1) * vnode_num_ + 1;
    for(size_t v = 0; v < vnode_num_; v++) {
      string m_str = node_mark + "#" + strfy(v);
      
      //auto h_val_org = hash_.HashForStr(m_str);
      //Key min_key = key_range_.start();
      //Key max_key = key_range_.end();
      //Key h_val_new = min_key + h_val_org % (max_key - min_key + 1);
      
     // Key h_val_new = hash_.HashToKeyRange(key_range_.start(), key_range_.end());
     // while(h_val_new == key_range_.start() || ring_.find(h_val_new) != ring_.end()) {
     //   h_val_new = hash_.HashToKeyRange(key_range_.start(), key_range_.end());
     // }
      Key h_val_new = hash_.AverageToKeyRange(key_range_.start(), key_range_.end(), num, i*vnode_num_ + v);
      CHECK_NE(h_val_new, key_range_.start());
      CHECK(ring_.find(h_val_new) == ring_.end());

      ring_[h_val_new] = m_str;
      inv_ring_[m_str] = h_val_new;
      
      num_tnodes++;
    }
  }

  // This is used to add a node (its uid is 0) to a specific position
  // We assume that this node doesn't have virtual nodes
  void HashRing::AddCoolNode(const std::string& node_mark) {
    Key cool_key = key_range_.start();
    string m_str = node_mark + "#" + strfy(0);
    ring_[cool_key] = m_str;
    num_tnodes++;
  }

  // Note that this method cannot be used to remove node with uid=0
  //void HashRing::RemoveNode(const std::string& node_mark) {
  //  for(size_t v = 0; v < vnode_num_; v++) {
  //    string m_str = node_mark + "#" + strfy(v);
  //    auto h_val_org = hash_.HashForStr(m_str);
  //    Key min_key = key_range_.start();
  //    Key max_key = key_range_.end();
  //    Key h_val_new = min_key + h_val_org % (max_key - min_key + 1);
  //    ring_.erase(h_val_new);
  //    num_tnodes--;
  //  }
  //}

  const std::string& HashRing::GetNode(const Key& key) {
    if(ring_.empty()) {
      // TODO Handle the error
      std::cerr << "[Error] GetNode error!!!" << std::endl;
      exit(1);
    }

    typename map<Key, std::string>::const_iterator iter;
    iter = ring_.upper_bound(key);
    if(iter == ring_.end()) {
      iter = ring_.begin();
    }
    return iter->second;
  }

  void HashRing::DivideKeyRange(Container* ctr, const KeyRange& whole, DHTInfo& dhtinfo) {
    KeyRange kr = whole;
    for(auto iter = ring_.begin(); iter != ring_.end(); iter++) {
      Key tmp_key = iter->first;
      std::vector<string> node_marks = split(iter->second, '#');
      uid_t tmp_id = std::stoi(node_marks[0]);
      if(tmp_id == 0) {
         KeyRange kr2(ring_.rbegin()->first, kr.end());
         Workload wl(tmp_id, kr2);
         dhtinfo.workloads_[make_pair(ctr->name(), tmp_id)] = wl;
       } else {
         auto iter = ring_.lower_bound(tmp_key);
         iter--;
         KeyRange kr2(iter->first, tmp_key);
         Workload wl(tmp_id, kr2);
         dhtinfo.workloads_[make_pair(ctr->name(), tmp_id)] = wl;
      }
    }
  }

  void HashRing::CollectBkpInfo(Container* ctr, DHTInfo& dhtinfo) {
    if(ring_.size() <= num_backup * num_replica)
      return;

    CHECK_GT(ring_.size(), num_backup * num_replica);
    for(auto iter = ring_.begin(); iter != ring_.end(); iter++) {
      auto tmp_iter = iter;

      vector<string> node_marks = split(iter->second, '#');
      uid_t tmp_id = std::stoi(node_marks[0]);

      std::vector<vector<uid_t>> id_2dArr;
      KeyRange kr = dhtinfo.workloads_[make_pair(ctr->name(), tmp_id)].key_range();
      for(size_t r = 0; r < num_replica; r++) {
        vector<uid_t> id_arr;
        for(size_t b = 0; b < num_backup; b++) {
          tmp_iter++;
          if(tmp_iter == ring_.end())
            tmp_iter = ring_.begin();

          vector<string> bkp_nodes = split(tmp_iter->second, '#');
          uid_t bkp_id = std::stoi(bkp_nodes[0]);
          id_arr.push_back(bkp_id);

          KeyRange kr_bkp = kr.EvenDivide(num_backup, b);
          if(!kr_bkp.Valid()) continue;
          Workload wl(bkp_id, kr_bkp);
          dhtinfo.bkp_workloads_[make_tuple(ctr->name(), tmp_id, bkp_id)] = wl;

        }
        id_2dArr.push_back(id_arr);
      }

      dhtinfo.bkp_nodes_[make_pair(ctr->name(), tmp_id)] = id_2dArr;
    }
  }

}

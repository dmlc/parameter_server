#pragma once

#include <map>
#include <tuple>
#include "system/workload.h"
#include "util/key.h"
#include "util/hashfunc.h"


namespace PS {

  DECLARE_int32(num_backup);
  DECLARE_int32(num_replica);

  typedef struct {
    map<pair<string, uid_t>, Workload> workloads_;
    map<pair<string, uid_t>, std::vector<std::vector<uid_t>>> bkp_nodes_;
    // map<tuple<container_name, main_node_uid, backup_node_uid>>
    map<std::tuple<string, uid_t, uid_t>, Workload> bkp_workloads_; 
  }DHTInfo;
  
  class Container;

  class HashRing {
    public:
      HashRing(size_t vnode_num) : vnode_num_(vnode_num) {
        num_backup = FLAGS_num_backup;
        num_replica = FLAGS_num_replica;
        num_tnodes = 0;
      }
      HashRing() { }

      void SetKeyRange(const KeyRange& key_range) {
        key_range_ = key_range;
      }

      void AddNode(const std::string& node_mark, const size_t n, const size_t i);
      void AddCoolNode(const std::string& node_mark);
      //void RemoveNode(const std::string& node_mark);
      const std::string& GetNode(const Key& key);

      void DivideKeyRange(Container* ctr, const KeyRange& whole, DHTInfo& dhtinfo);
      void CollectBkpInfo(Container* ctr, DHTInfo& dhtinfo);


      // for test only
      std::map<Key, std::string>& GetRing() {return ring_;}

    private:
      //hash_val, node_mark_str
      std::map<Key, std::string> ring_;
      std::map<std::string, Key> inv_ring_;
      HashFunc hash_;
      KeyRange key_range_;
      
      // the number of virtual nodes per physical node
      size_t vnode_num_;
      // the number of all virtual nodes
      size_t num_tnodes;
      
      size_t num_backup;
      size_t num_replica;
        
  };

}

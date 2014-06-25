#include "gtest/gtest.h"
#include "base/shared_array.h"
#include "util/kv_partition.h"
using namespace PS;

template <typename K, typename V>
void verify(const SArray<K> &key, const SArray<V> &val, K pivot, size_t first_length) {
  for (size_t i = 0; i < first_length; i++) {
    ASSERT_LT(key[i], pivot) << "key[" << i << "] not in first group";
    ASSERT_EQ(key[i], val[i])
          << "(" << key[i] << "," << val[i] << "): key and value not sorted";
  }
  for (size_t i = first_length; i < key.size();i++) {
    ASSERT_LE(pivot, key[i]) << "key[" << i << "] not in second group";
    ASSERT_EQ(key[i], val[i])
          << "(" << key[i] << "," << val[i] << "): key and value not sorted";
  }
}

template <typename K, typename V>
void verify(const SArray<K> &key, const SArray<V> &val, std::vector<K> partition, size_t *count) {
  size_t sum = 0;
  for (size_t i = 0; i < partition.size()-1; i++) {
    sum += count[i];
    verify(key, val, partition[i + 1], sum);
  }
} 

TEST(PARTITION, TWOWAYPARTITION_REGULAR) {
  using namespace PS;
 
  SArray<uint64> key({1, 20, 2, 49, 8, 39 , 5, 19, 7, 18});
  SArray<uint32> val({1, 20, 2, 49, 8, 39 , 5, 19, 7, 18});
  
  auto first_length = partition(key, val, (size_t) 0, key.size(), (uint64) 10);
  
  ASSERT_EQ(first_length, 5);
  verify(key,val,(uint64)10, first_length); 
  
}

TEST(PARTITION, TWOWAYPARTITION_LEFTALL) {
  using namespace PS;
  SArray<uint64> key({1, 20, 2, 49, 8, 39 , 5, 19, 7, 18});
  SArray<uint32> val({1, 20, 2, 49, 8, 39 , 5, 19, 7, 18});
  
  auto first_length = partition(key, val, (size_t) 0, key.size(), (uint64) 50);
  
  ASSERT_EQ(first_length, 10);
  verify(key,val, (uint64)50, first_length); 
  
}

TEST(PARTITION, TWOWAYPARTITION_RIGHTALL) {
  using namespace PS;
  SArray<uint64> key({1, 20, 2, 49, 8, 39 , 5, 19, 7, 18});
  SArray<uint32> val({1, 20, 2, 49, 8, 39 , 5, 19, 7, 18});
  
  auto first_length = partition(key, val, (size_t) 0, key.size(), (uint64) 0);
  
  ASSERT_EQ(first_length, 0);
  verify(key,val, (uint64)0, first_length); 
  
}

TEST(PARTITION, KWAY_PARTITION) {
  using namespace PS;
  SArray<uint64> key({1, 20, 2, 49, 8, 39 , 5, 19, 7, 18});
  SArray<uint32> val({1, 20, 2, 49, 8, 39 , 5, 19, 7, 18});
  
  std::vector<uint64> part({0, 5, 10, 30, 40, 50});
  size_t bin_size[5];
  
  k_partition(key,val, (size_t) 0,key.size(), part, (size_t) 0, part.size() - 1, bin_size);
  
  verify(key,val,part, bin_size); 
  
}

TEST(PARTITION, KWAY_PARTITION_KEYONLY) {
  using namespace PS;
  SArray<uint64> key({1, 20, 2, 49, 8, 39 , 5, 19, 7, 18});
  
  std::vector<uint64> part({0, 5, 10, 30, 40, 50});
  size_t bin_size[5];
  
  k_partition(key, (size_t) 0,key.size(), part, (size_t) 0, part.size() - 1, bin_size);
  
  verify(key,key,part, bin_size); 
  
}






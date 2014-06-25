#pragma once
#include "base/shared_array.h"

namespace PS {
template <typename K, typename V> 
size_t partition(SArray<K> key, SArray<V> *value, size_t n_value, size_t first, size_t last, K pivot) {
    //kv partition
    //partitions range [first, last) into two (elements < pivot, elements >= pivot)
    //returns index of first element on second partition.

    while (first!=last) {
        while (key[first] < pivot) {
            ++first;
            if (first==last) return first;
        }
        do {
            --last;
            if (first==last) return first;
        } while (key[last] >= pivot);
        std::swap (key[first],key[last]);
        for (size_t i = 0; i < n_value; i++) {
            std::swap (value[i][first],value[i][last]);
        }
        ++first;
    }
    return first;
}
template <typename K>
void k_partition(SArray<K> key, size_t first, size_t last,
                 const std::vector<K>& partition, size_t partition_first, size_t partition_last,
                 size_t* count) {

    if (partition_last - partition_first == 1) {
        // we are done
        count[partition_first] = last - first;
        return;
    }
    if (first == last) {
        // No entries in this partition
        for (size_t i = partition_first; i < partition_last; i++ ) {
            count[i] = 0;
        }
        return;
    }
    size_t partition_mid = partition_first + (partition_last - partition_first)/2;
    K pivot = partition[partition_mid];
    K *ptr = std::partition(key.begin() + first, key.begin() + last, [pivot](K i){ return (i < pivot);});
    size_t mid = first + (ptr - (key.begin() + first) );
    k_partition(key, first, mid,
                partition, partition_first, partition_mid, count);
    k_partition(key, mid, last,
                partition, partition_mid, partition_last, count);
}


template <typename K, typename V>
void k_partition(SArray<K> key, SArray<V> *value, size_t n_value,  size_t first, size_t last,
                 const std::vector<K>& partition, size_t partition_first, size_t partition_last,
                 size_t* count) {

    if (partition_last - partition_first == 1) {
        // we are done
        count[partition_first] = last - first;
        return;
    }
    if (first == last) {
        // No entries in this partition
        for (size_t i = partition_first; i < partition_last; i++ ) {
            count[i] = 0;
        }
        return;
    }
    size_t partition_mid = partition_first + (partition_last - partition_first)/2;
    size_t mid = PS::partition(key, value, n_value, first, last, partition[partition_mid]);
    
    k_partition(key, value, n_value, first, mid,
                partition, partition_first, partition_mid, count);
    k_partition(key, value, n_value, mid, last,
                partition, partition_mid, partition_last, count);
}

}

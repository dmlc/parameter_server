#pragma once
#include "base/shared_array.h"
namespace PS {

template <typename K, typename V, class Op>
void parallelOrderedMatch(
    const K* src_key, const K* src_key_end, const V* src_val,
    const K* dst_key, const K* dst_key_end, V* dst_val,
    size_t grainsize, size_t* n) {
  size_t src_len = std::distance(src_key, src_key_end);
  size_t dst_len = std::distance(dst_key, dst_key_end);
  if (dst_len == 0 || src_len == 0) return;

  // drop the unmatched tail of src
  src_key = std::lower_bound(src_key, src_key_end, *dst_key);
  src_val += src_key - (src_key_end - src_len);

  Op op;
  if (dst_len <= grainsize) {
    while (dst_key != dst_key_end && src_key != src_key_end) {
      if (*src_key < *dst_key) {
        ++ src_key; ++ src_val;
      } else {
        if (!(*dst_key < *src_key)) {
          op(src_val, dst_val);
          ++ src_key; ++ src_val;
          ++ (*n);
        }
        ++ dst_key; ++ dst_val;
      }
    }
  } else {
    std::thread thr(
        parallelOrderedMatch<K,V,Op>, src_key, src_key_end, src_val,
        dst_key, dst_key + dst_len / 2, dst_val, grainsize, n);
    size_t m = 0;
    parallelOrderedMatch<K,V,Op>(
        src_key, src_key_end, src_val,
        dst_key + dst_len / 2, dst_key_end, dst_val + dst_len / 2,
        grainsize, &m);
    thr.join();
    *n += m;
  }
}


// TODO unify with class Operator
template<typename T> struct OpAssign {
  virtual void operator()(const T* src, T* dst) const { *dst = *src; }
};

template<typename T> struct OpPlus {
  void operator()(const T* src, T* dst) const { *dst += *src; }
};

template<typename T> struct OpOr {
  void operator()(const T* src, T* dst) const { *dst |= *src; }
};

// assume both src_key and dst_key are ordered, apply
//  op(src_val[i], dst_val[j]) if src_key[i] = dst_key[j]
template <typename K, typename V, class Op = OpAssign<V>>
size_t parallelOrderedMatch(
    const SArray<K>& src_key,
    const SArray<V>& src_val,
    const SArray<K>& dst_key,
    SArray<V>* dst_val,
    int num_threads = FLAGS_num_threads) {
  // do check
  CHECK_GT(num_threads, 0);
  CHECK_EQ(src_key.size(), src_val.size());
  if (dst_val->empty()) {
    dst_val->resize(dst_key.size());
    dst_val->setZero();
  } else {
    CHECK_EQ(dst_val->size(), dst_key.size());
  }
  SizeR range = dst_key.findRange(src_key.range());
  size_t grainsize = std::max(range.size() / num_threads + 5, (size_t)1024*1024);
  size_t n = 0;
  parallelOrderedMatch<K, V, Op>(
      src_key.begin(), src_key.end(), src_val.begin(),
      dst_key.begin() + range.begin(), dst_key.begin() + range.end(),
      dst_val->begin() + range.begin(), grainsize, &n);
  return n;
}

// assume both src_key and dst_key are ordered,
template <typename K, typename V, class Op = OpPlus<V>>
void parallelUnion(
    const SArray<K>& key1,
    const SArray<V>& val1,
    const SArray<K>& key2,
    const SArray<V>& val2,
    SArray<K>* joined_key,
    SArray<V>* joined_val,
    int num_threads = FLAGS_num_threads) {
  CHECK_NOTNULL(joined_key);
  CHECK_NOTNULL(joined_val);
  *joined_key = key1.setUnion(key2);
  joined_val->resize(0);
  auto n1 = parallelOrderedMatch<K,V,Op>(
      key1, val1, *joined_key, joined_val, num_threads);
  CHECK_EQ(n1, key1.size());

  auto n2 = parallelOrderedMatch<K,V,Op>(
      key2, val2, *joined_key, joined_val, num_threads);
  CHECK_EQ(n2, key2.size());
}


} // namespace PS


// template <typename V> using AlignedArray = std::pair<SizeR, SArray<V>>;
// template <typename V> using AlignedArrayList = std::vector<AlignedArray<V>>;

// enum class MatchOperation : unsigned char {
//   ASSIGN = 0,
//   ADD,
//   NUM
// };

// template <typename K, typename V>
// static void match(
//   const SizeR &dst_key_pos_range,
//   const SArray<K> &dst_key,
//   SArray<V> &dst_val,
//   const SArray<K> &src_key,
//   const SArray<V> &src_val,
//   size_t *matched,
//   const MatchOperation op) {

//   *matched = 0;
//   if (dst_key.empty() || src_key.empty()) {
//     return;
//   }

//   std::unique_ptr<size_t[]> matched_array_ptr(new size_t[FLAGS_num_threads]);
//   {
//     // threads
//     ThreadPool pool(FLAGS_num_threads);
//     for (size_t thread_idx = 0; thread_idx < FLAGS_num_threads; ++thread_idx) {
//       pool.add([&, thread_idx]() {
//         // matched ptr
//         size_t *my_matched = &(matched_array_ptr[thread_idx]);
//         *my_matched = 0;

//         // partition dst_key_pos_range evenly
//         SizeR my_dst_key_pos_range = dst_key_pos_range.evenDivide(
//           FLAGS_num_threads, thread_idx);
//         // take the remainder if dst_key_range is indivisible by threads number
//         if (FLAGS_num_threads - 1 == thread_idx) {
//           my_dst_key_pos_range.set(
//             my_dst_key_pos_range.begin(), dst_key_pos_range.end());
//         }

//         // iterators for dst
//         const K *dst_key_it = dst_key.data() + my_dst_key_pos_range.begin();
//         const K* dst_key_end = dst_key.data() + my_dst_key_pos_range.end();
//         V *dst_val_it = dst_val.data() + (
//           my_dst_key_pos_range.begin() - dst_key_pos_range.begin());

//         // iterators for src
//         // const K *src_key_it = src_key.data();
//         // const V *src_val_it = src_val.data();
//         const K *src_key_it = std::lower_bound(src_key.begin(), src_key.end(), *dst_key_it);
//         const K *src_key_end = std::upper_bound(src_key.begin(), src_key.end(), *(dst_key_end - 1));
//         const V *src_val_it = src_val.begin() + (src_key_it - src_key.begin());

//         // clear dst_val if necessary
//         if (MatchOperation::ASSIGN == op) {
//           memset(dst_val_it, 0, sizeof(V) * (dst_key_end - dst_key_it));
//         }

//         // traverse
//         while (dst_key_end != dst_key_it && src_key_end != src_key_it) {
//           if (*src_key_it < *dst_key_it) {
//             // forward iterators for src
//             ++src_key_it;
//             ++src_val_it;
//           } else {
//             if (!(*dst_key_it < *src_key_it)) {
//               // equals
//               if (MatchOperation::ASSIGN == op) {
//                 *dst_val_it = *src_val_it;
//               } else if (MatchOperation::ADD == op) {
//                 *dst_val_it += *src_val_it;
//               } else {
//                 LL << "BAD MatchOperation [" << static_cast<int32>(op) << "]";
//                 throw std::runtime_error("BAD MatchOperation");
//               }

//               // forward iterators for src
//               ++src_key_it;
//               ++src_val_it;
//               ++(*my_matched);
//             }

//             // forward iterators for dst
//             ++dst_key_it;
//             ++dst_val_it;
//           }
//         }
//       });
//     }
//     pool.startWorkers();
//   }

//   // reduce matched count
//   for (size_t i = 0; i < FLAGS_num_threads; ++i) {
//     *matched += matched_array_ptr[i];
//   }

//   return;
// }

// // TODO multithread version
// template <typename K, typename V>
// static AlignedArray<V> oldMatch(
//     const SArray<K>& dst_key, const SArray<K>& src_key, V* src_val,
//     Range<K> src_key_range, size_t* matched) {
//   // if (src_key_range == Range<K>::all())
//   //   src_key_range = src_key.range();
//   *matched = 0;
//   if (dst_key.empty() || src_key.empty()) {
//     return std::make_pair(SizeR(), SArray<V>());
//   }

//   SizeR range = dst_key.findRange(src_key_range);

//   SArray<V> value(range.size());
//   V* dst_val = value.data();
//   memset(dst_val, 0, sizeof(V)*value.size());

//   // optimization, binary search the start point
//   const K* dst_key_it = dst_key.begin() + range.begin();
//   const K* src_key_it = std::lower_bound(src_key.begin(), src_key.end(), *dst_key_it);
//   src_val += src_key_it - src_key.begin();

//   while (dst_key_it != dst_key.end() && src_key_it != src_key.end()) {
//     if (*src_key_it < *dst_key_it) {
//       ++ src_key_it;
//       ++ src_val;
//     } else {
//       if (!(*dst_key_it < *src_key_it)) {
//         *dst_val = *src_val;
//         ++ src_key_it;
//         ++ src_val;
//         ++ *matched;
//       }
//       ++ dst_key_it;
//       ++ dst_val;
//     }
//   }
//   return std::make_pair(range, value);
// }

// some old versions

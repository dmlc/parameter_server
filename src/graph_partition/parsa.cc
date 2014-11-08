#include "graph_partition/parsa.h"
#include "data/stream_reader.h"
#include "base/localizer.h"
namespace PS {

void Parsa::init() {
  conf_ = app_cf_.parsa();
  conf_.mutable_input_graph()->set_ignore_feature_group(true);
  num_partitions_ = conf_.num_partitions();
  num_V_ = conf_.v_size();
  neighbor_set_.resize(num_partitions_);
  // for (int k = 0; k < num_partitions_; ++k) {
  //   neighbor_set_[k].assigned_V.resize(num_V_);
  // }
}

void Parsa::run() {
  partition();
}

void Parsa::partition() {
  // reader
  using std::get;
  typedef std::tuple<GraphPtr, GraphPtr, SArray<Key>, ExampleListPtr> DataPair;
  StreamReader<Empty> stream(searchFiles(conf_.input_graph()));
  ProducerConsumer<DataPair> reader(conf_.data_buff_size_in_mb());
  reader.startProducer([this, &stream](DataPair* data, size_t* size)->bool {
      MatrixPtrList<Empty> X;
      auto examples = ExampleListPtr(new ExampleList());
      bool ret = stream.readMatrices(conf_.block_size(), &X, examples.get());
      if (X.empty()) return false;

      // localize
      auto G = std::static_pointer_cast<SparseMatrix<Key,Empty>>(X.back());
      G->info().set_type(MatrixInfo::SPARSE_BINARY);
      G->value().clear();
      Localizer<Key, Empty> localizer;
      SArray<Key> key;
      localizer.countUniqIndex(G, &key);

      get<0>(*data) = std::static_pointer_cast<Graph>(localizer.remapIndex(key));
      get<1>(*data) = std::static_pointer_cast<Graph>(get<0>(*data)->toColMajor());
      get<2>(*data) = key;
      get<3>(*data) = examples;

      *size = 1;
      return ret;
    });

  // writer

  typedef std::pair<ExampleListPtr, SArray<int>> ResultPair;
  ProducerConsumer<ResultPair> writer_1;
  std::vector<RecordWriter> proto_writers_1;
  std::vector<DataConfig> tmp_files;
  proto_writers_1.resize(num_partitions_);
  for (int i = 0; i < num_partitions_; ++i) {
    tmp_files.push_back(ithFile(conf_.output_graph(), 0, "_part_"+to_string(i)+"_tmp"));
    auto file = File::openOrDie(tmp_files.back(), "w");
    proto_writers_1[i] = RecordWriter(file);
  }
  writer_1.setCapacity(conf_.data_buff_size_in_mb());
  writer_1.startConsumer([&proto_writers_1](const ResultPair& data) {
      const auto& examples = *data.first;
      const auto& partition = data.second;
      CHECK_EQ(examples.size(), partition.size());
      for (int i = 0; i < examples.size(); ++i) {
        CHECK(proto_writers_1[partition[i]].WriteProtocolMessage(examples[i]));
      }
    });

  // partition U
  DataPair data;
  SArray<int> map_U;
  int i = 0;
  while (reader.pop(&data)) {
    partitionU(get<0>(data), get<1>(data), get<2>(data), &map_U);
    writer_1.push(std::make_pair(get<3>(data), map_U));
    LL << i ++;
  }
  writer_1.setFinished();
  writer_1.waitConsumer();

  // partition V
  partitionV();

  // reader & write
  // uint64 cost = 0;
  // for (int k = 0; k < num_partitions_; ++k) {
  //   std::unordered_set<Key> key_set;
  //   auto file = File::openOrDie(tmp_files[k], "r");
  //   RecordReader proto_reader(file);
  //   Example ex;
  //   while (proto_reader.ReadProtocolMessage(&ex)) {
  //     for (int i = 0;  i < ex.slot_size(); ++i) {
  //       const auto& slot = ex.slot(i);
  //       if (slot.id() == 0) continue;
  //       for (int j = 0; j < slot.key_size(); ++j) {
  //         auto key = slot.key(j);
  //         if (partition_V_[hash(key)] != k) {
  //           if (key_set.count(key) == 0) {
  //             key_set.insert(key);
  //             ++ cost;
  //           }
  //         }
  //       }
  //     }
  //   }
  // }
  // LL << cost;


  // producerConsumer<ResultPair> writer_2;
  // typedef std::pair<GraphPtrList, ExampleListPtr> DataPair;
  // StreamReader<Empty> stream(searchFiles(conf_.input_graph()));
  // ProducerConsumer<DataPair> reader(conf_.data_buff_size_in_mb());
  // reader.startProducer([this, &stream](DataPair* data, size_t* size)->bool {
}

void Parsa::partitionV() {
  // sort the neighbor set
  std::vector<std::vector<Key>> ordered_nbset(num_partitions_);
  for (int k = 0; k < num_partitions_; ++k) {
    auto& part = ordered_nbset[k];
    part.resize(neighbor_set_[k].assigned_V.size());
    int i = 0;
    for (const auto& e : neighbor_set_[k].assigned_V) {
      part[i++] = e;
    }
    sort(part.begin(), part.end());
  }

  // partitioning
  std::vector<std::vector<Key>::iterator> its(num_partitions_);
  for (int k = 0; k < num_partitions_; ++k) {
    its[k] = ordered_nbset[k].begin();
  }
  std::vector<int> cost(num_partitions_);
  bool done = false;
  while (!done) {
    Key cur = kMaxKey;
    for (int k = 0; k < num_partitions_; ++k) {
      if (its[k] != ordered_nbset[k].end() && *its[k] < cur) cur = *its[k];
    }
    if (cur == kMaxKey) break;
    int max_cost = 0;
    int best_k = -1;
    for (int k = 0; k < num_partitions_; ++k) {
      if (*its[k] == cur) {
        int c = ++ cost[k];
        if (c > max_cost) { max_cost = c; best_k = k; }
        ++ its[k];
      }
    }
    CHECK_GE(best_k, 0);
    partition_V_[cur] = best_k;
    -- cost[best_k];
  }

  // for (auto a)

  // partition_V_.resize(num_V_);
  // for (int i = 0; i < num_V_; ++i) {
  //   int max_v = 0;
  //   int max_j = -1;
  //   for (int j = 0; j <
  //     if (neighbor_set_[j].assigned_V[i]) {
  //       int v = ++ cost[j];
  //       if (v > max_v) { max_j = j; max_v = v; }
  //     }
  //   }
  //   /// assign V_j to partitionn max_j
  //   if (max_j >= 0) -- cost[max_j];
  //   partition_V_[i] = max_j;
  // }

  int v = 0;
  for (int j = 0; j < num_partitions_; ++j) v += cost[j];
  LL << v;
}
void Parsa::partitionU(const GraphPtr& row_major_blk, const GraphPtr& col_major_blk,
                       const SArray<Key>& global_key, SArray<int>* map_U) {
  // initialization
  int n = row_major_blk->rows();
  map_U->resize(n);
  assigned_U_.clear();
  assigned_U_.resize(n);
  initCost(row_major_blk, global_key);

  // partitioning
  for (int i = 0; i < n; ++i) {
    // TODO sync if necessary

    // assing U_i to partition k
    int k = i % num_partitions_;
    int Ui = cost_[k].minIdx();
    assigned_U_.set(Ui);
    (*map_U)[Ui] = k;

    // update
    updateCostAndNeighborSet(row_major_blk, col_major_blk, global_key, Ui, k);
  }
}

// init the cost of assigning U_i to partition k
void Parsa::initCost(const GraphPtr& row_major_blk, const SArray<Key>& global_key) {
  int n = row_major_blk->rows();
  size_t* row_os = row_major_blk->offset().begin();
  uint32* row_idx = row_major_blk->index().begin();
  std::vector<int> cost(n);
  cost_.resize(num_partitions_);
  for (int k = 0; k < num_partitions_; ++k) {
    const auto& assigned_V = neighbor_set_[k].assigned_V;
    for (int i = 0; i < n; ++ i) {
      for (size_t j = row_os[i]; j < row_os[i+1]; ++j) {
       // cost[i] += !assigned_V[row_idx[j]];
        cost[i] += assigned_V.count(global_key[row_idx[j]]) == 0;
      }
    }
    cost_[k].init(cost, conf_.cost_cache_limit());
  }
}

void Parsa::updateCostAndNeighborSet(
    const GraphPtr& row_major_blk, const GraphPtr& col_major_blk,
    const SArray<Key>& global_key, int Ui, int partition) {
  for (int s = 0; s < num_partitions_; ++s) cost_[s].remove(Ui);

  size_t* row_os = row_major_blk->offset().begin();
  size_t* col_os = col_major_blk->offset().begin();
  uint32* row_idx = row_major_blk->index().begin();
  uint32* col_idx = col_major_blk->index().begin();
  auto& assigned_V = neighbor_set_[partition].assigned_V;
  auto& cost = cost_[partition];
  for (size_t i = row_os[Ui]; i < row_os[Ui+1]; ++i) {
    int Vj = row_idx[i];
    // if (assigned_V[Vj]) continue;
    // assigned_V.set(Vj);
    auto key = global_key[Vj];
    if (assigned_V.count(key) != 0) continue;
    assigned_V.insert(key);
    for (size_t s = col_os[Vj]; s < col_os[Vj+1]; ++s) {
      int Uk = col_idx[s];
      if (assigned_U_[Uk]) continue;
      cost.decrAndSort(Uk);
    }
  }
}

} // namespace PS

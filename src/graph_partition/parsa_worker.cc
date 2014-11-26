#include "graph_partition/parsa_worker.h"
#include "base/localizer.h"
namespace PS {
namespace GP {

void ParsaWorker::init() {
  GraphPartition::init();
  conf_.mutable_input_graph()->set_ignore_feature_group(true);
  num_partitions_ = conf_.parsa().num_partitions();
  neighbor_set_.resize(num_partitions_);
  random_partition_ = conf_.parsa().randomly_partition_u();
  sync_nbset_ = KVVectorPtr<Key, uint64>(new KVVector<Key, uint64>());
  REGISTER_CUSTOMER(app_cf_.parameter_name(0), sync_nbset_);
}

void ParsaWorker::readGraph(
    StreamReader<Empty>& reader, ProducerConsumer<BlockData>& producer,
    int& start_id, int end_id, int block_size, bool keep_examples) {
  CHECK_GT(end_id, start_id);
  producer.startProducer(
      [this, &reader, &start_id, end_id, block_size, keep_examples] (
          BlockData* blk, size_t* size)->bool {
        // read a block
        MatrixPtrList<Empty> X;
        auto examples = ExampleListPtr(new ExampleList());
        bool ret = reader.readMatrices(
            block_size, &X, keep_examples ? examples.get() : nullptr);
        if (X.empty()) return false;

        // find the unique keys
        auto G = std::static_pointer_cast<SparseMatrix<Key,Empty>>(X.back());
        G->info().set_type(MatrixInfo::SPARSE_BINARY);
        G->value().clear();
        Localizer<Key, Empty> localizer;
        SArray<Key> key;
        localizer.countUniqIndex(G, &key);

        // pull the current partition from servers
        sync_nbset_->key(start_id) = key;
        int time = -1;
        if (!no_sync_) {
          MessagePtr pull(new Message(kServerGroup));
          pull->task.set_key_channel(start_id);
          pull->setKey(key);
          pull->addFilter(FilterConfig::KEY_CACHING);
          time = sync_nbset_->pull(pull);
        }

        // preprocess data and store the results
        blk->row_major  = std::static_pointer_cast<Graph>(localizer.remapIndex(key));
        blk->col_major  = std::static_pointer_cast<Graph>(blk->row_major->toColMajor());
        blk->examples   = examples;
        blk->pull_time  = time;
        blk->blk_id     = start_id ++;
        *size           = 1;  // a fake number, i'm lazy to get the correct one

        return start_id == end_id ? false : ret;
      });
}


void ParsaWorker::stage0() {
  auto parsa = conf_.parsa();
  no_sync_ = true;
  delta_nbset_ = false;

  // warm up
  if (parsa.stage0_warm_up_blocks()) {
    StreamReader<Empty> stream_0(conf_.input_graph());
    ProducerConsumer<BlockData> reader_0(parsa.data_buff_size_in_mb());
    int start_id_0 = 0;
    int end_id_0 = start_id_0 + parsa.stage0_warm_up_blocks();
    readGraph(stream_0, reader_0, start_id_0, end_id_0, parsa.stage0_block_size(), false);

    BlockData blk;
    while (reader_0.pop(&blk)) {
      auto& value = sync_nbset_->value(blk.blk_id);
      parallelOrderedMatch(
          added_nbset_key_, added_nbset_value_, sync_nbset_->key(blk.blk_id),
          OpAssign<uint64>(), FLAGS_num_threads, &value);
      partitionU(blk, nullptr);
    }
    LL << "stage 0: initialized by " << start_id_0 << " blocks";
  }

  // real work
  if (parsa.stage0_blocks()) {
    StreamReader<Empty> stream_1(conf_.input_graph());
    ProducerConsumer<BlockData> reader_1(parsa.data_buff_size_in_mb());
    int start_id_1 = parsa.stage0_warm_up_blocks();
    int end_id_1 = start_id_1 + parsa.stage0_blocks();
    readGraph(stream_1, reader_1, start_id_1, end_id_1, parsa.stage0_block_size(), false);

    BlockData blk;
    SArray<Key> nbset_key;
    SArray<uint64> nbset_value;
    while (reader_1.pop(&blk)) {
      auto& value = sync_nbset_->value(blk.blk_id);
      if (blk.blk_id == parsa.stage0_blocks()) {
        parallelOrderedMatch(
            added_nbset_key_, added_nbset_value_, sync_nbset_->key(blk.blk_id),
            OpOr<uint64>(), FLAGS_num_threads, &value);
      } else {
        SArray<Key> tmp_key;
        SArray<uint64> tmp_value;
        parallelUnion(
            nbset_key, nbset_value, added_nbset_key_, added_nbset_value_,
            OpOr<uint64>(), FLAGS_num_threads, &tmp_key, &tmp_value);
        nbset_key = tmp_key;
        nbset_value = tmp_value;

        parallelOrderedMatch(
            nbset_key, nbset_value, sync_nbset_->key(blk.blk_id),
            OpOr<uint64>(), FLAGS_num_threads, &value);

        delta_nbset_ = true;
      }

      partitionU(blk, nullptr);
    }

    MessagePtr push(new Message(kServerGroup));
    push->task.set_key_channel(end_id_1);
    push->setKey(nbset_key);
    push->addValue(nbset_value);
    push->wait = true;
    sync_nbset_->set(push)->set_op(Operator::OR);
    sync_nbset_->push(push);
    LL << "stage 0: partitioned " << start_id_1 - parsa.stage0_warm_up_blocks() << " blocks";
  }

}


void ParsaWorker::stage1() {

}
// void ParsaWorker::partitionU() {
//   int blk_id = 0;


//   // writer
//   typedef std::pair<ExampleListPtr, SArray<int>> ResultPair;
//   ProducerConsumer<ResultPair> writer_1;
//   std::vector<RecordWriter> proto_writers_1;
//   std::vector<DataConfig> tmp_files;
//   proto_writers_1.resize(num_partitions_);
//   for (int i = 0; i < num_partitions_; ++i) {
//     tmp_files.push_back(ithFile(conf_.output_graph(), 0, "_part_"+to_string(i)+"_tmp"));
//     auto file = File::openOrDie(tmp_files.back(), "w");
//     proto_writers_1[i] = RecordWriter(file);
//   }
//   writer_1.setCapacity(conf_.parsa().data_buff_size_in_mb());
//   writer_1.startConsumer([&proto_writers_1](const ResultPair& data) {
//       const auto& examples = *data.first;
//       const auto& partition = data.second;
//       CHECK_EQ(examples.size(), partition.size());
//       for (int i = 0; i < examples.size(); ++i) {
//         CHECK(proto_writers_1[partition[i]].WriteProtocolMessage(examples[i]));
//       }
//     });

//   // partition U
//   BlockData blk;
//   SArray<int> map_U;
//   // int i = 0;
//   while (reader.pop(&blk)) {
//     partitionU(blk, &map_U);
//     writer_1.push(std::make_pair(blk.examples, map_U));
//     // LL << i ++;
//   }
//   writer_1.setFinished();
//   writer_1.waitConsumer();


//   // producerConsumer<ResultPair> writer_2;
//   // typedef std::pair<GraphPtrList, ExampleListPtr> DataPair;
//   // StreamReader<Empty> stream(searchFiles(conf_.input_graph()));
//   // ProducerConsumer<DataPair> reader(conf_.data_buff_size_in_mb());
//   // reader.startProducer([this, &stream](DataPair* data, size_t* size)->bool {
// }

void ParsaWorker::partitionU(const BlockData& blk, SArray<int>* map_U) {
  if (blk.pull_time >= 0) {
    sync_nbset_->waitOutMsg(kServerGroup, blk.pull_time);
  }
  int id = blk.blk_id;
  auto key = sync_nbset_->key(id);
  initNeighborSet(key, sync_nbset_->value(id));

  int n = blk.row_major->rows();
  if (map_U) map_U->resize(n);
  assigned_U_.clear();
  assigned_U_.resize(n);
  initCost(blk.row_major, key);

  std::vector<int> rnd_idx;
  if (random_partition_) {
    rnd_idx.resize(n);
    for (int i = 0; i < n; ++i) rnd_idx[i] = i;
    srand(time(NULL));
    random_shuffle(rnd_idx.begin(), rnd_idx.end());
  }

  // partitioning
  for (int i = 0; i < n; ++i) {
    // assing U_i to partition k
    int k = i % num_partitions_;
    int Ui = random_partition_ ? rnd_idx[i] : cost_[k].minIdx();
    assigned_U_.set(Ui);
    if (map_U) (*map_U)[Ui] = k;

    // update
    updateCostAndNeighborSet(blk.row_major, blk.col_major, key, Ui, k);
  }

  // send results to servers
  sendUpdatedNeighborSet(id);
  sync_nbset_->clear(id);
}

void ParsaWorker::initNeighborSet(
    const SArray<Key>& global_key, const SArray<uint64>& nbset) {
  CHECK_EQ(global_key.size(), nbset.size());
  int n = global_key.size();

  added_neighbor_set_.resize(0);
  for (int i = 0; i < num_partitions_; ++i) {
#ifdef EXACT_NBSET
    neighbor_set_[i].clear();
#else
    int k = conf_.parsa().bloomfilter_k();
    int m = n * k * 1.44 * conf_.parsa().bloomfilter_m_ratio();
    neighbor_set_[i].resize(m, k);
#endif
  }

  for (int i = 0; i < n; ++i) {
    uint64 s = nbset[i];
    if (s == 0) continue;
    for (int k = 0; k < num_partitions_; ++k) {
      if (s & (1 << k)) {
        neighbor_set_[k].insert(global_key[i]);
      }
    }
  }
}

void ParsaWorker::sendUpdatedNeighborSet(int blk_id) {
  auto& nbset = added_neighbor_set_;
  if (nbset.empty()) return;

  // pack the local updates
  std::sort(nbset.begin(), nbset.end(),
            [](const KP& a, const KP& b) { return a.first < b.first; });

  added_nbset_key_.resize(0);
  added_nbset_value_.resize(0);
  Key pre = nbset[0].first;
  uint64 s = 0;
  for (int i = 0; i < nbset.size(); ++i) {
    Key cur = nbset[i].first;
    if (cur != pre) {
      added_nbset_value_.pushBack(s);
      added_nbset_key_.pushBack(pre);
      pre = cur;
      s = 0;
    }
    s |= 1 << nbset[i].second;
  }
  added_nbset_key_.pushBack(nbset.back().first);
  added_nbset_value_.pushBack(s);
  // LL << added_nbset_key_.size();

  // send local updates
  if (!no_sync_) {
    MessagePtr push(new Message(kServerGroup));
    push->task.set_key_channel(blk_id);
    push->setKey(added_nbset_key_);
    push->addValue(added_nbset_value_);
    push->addFilter(FilterConfig::KEY_CACHING)->set_clear_cache_if_done(true);
    sync_nbset_->set(push)->set_op(Operator::OR);
    sync_nbset_->push(push);
    // TODO store push time
  }
}

// init the cost of assigning U_i to partition k
void ParsaWorker::initCost(const GraphPtr& row_major_blk, const SArray<Key>& global_key) {
  cost_.resize(num_partitions_);
  if (random_partition_) return;
  int n = row_major_blk->rows();
  size_t* row_os = row_major_blk->offset().begin();
  uint32* row_idx = row_major_blk->index().begin();
  for (int k = 0; k < num_partitions_; ++k) {
    std::vector<int> cost(n);
    const auto& assigned_V = neighbor_set_[k];
    for (int i = 0; i < n; ++ i) {
      for (size_t j = row_os[i]; j < row_os[i+1]; ++j) {
       // cost[i] += !assigned_V[global_key[row_idx[j]]];
        cost[i] += !assigned_V.count(global_key[row_idx[j]]);
      }
    }
    cost_[k].init(cost, conf_.parsa().max_cached_cost_value());
  }
}

void ParsaWorker::updateCostAndNeighborSet(
    const GraphPtr& row_major_blk, const GraphPtr& col_major_blk,
    const SArray<Key>& global_key, int Ui, int partition) {
  if (!random_partition_) {
    for (int s = 0; s < num_partitions_; ++s) cost_[s].remove(Ui);
  }

  size_t* row_os = row_major_blk->offset().begin();
  size_t* col_os = col_major_blk->offset().begin();
  uint32* row_idx = row_major_blk->index().begin();
  uint32* col_idx = col_major_blk->index().begin();
  auto& assigned_V = neighbor_set_[partition];
  auto& cost = cost_[partition];
  for (size_t i = row_os[Ui]; i < row_os[Ui+1]; ++i) {
    int Vj = row_idx[i];
    auto key = global_key[Vj];
    bool assigned = assigned_V.count(key);
    if (!(delta_nbset_ && assigned)) {
      added_neighbor_set_.pushBack(std::make_pair(key, (P)partition));
    }
    if (assigned) continue;
    assigned_V.insert(key);
    for (size_t s = col_os[Vj]; s < col_os[Vj+1]; ++s) {
      int Uk = col_idx[s];
      if (assigned_U_[Uk]) continue;
      if (!random_partition_) cost.decrAndSort(Uk);
    }
  }
}

} // namespace GP
} // namespace PS

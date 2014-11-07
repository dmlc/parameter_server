#include "graph_partition/parsa.h"
#include "data/stream_reader.h"

namespace PS {

void Parsa::init() {
  conf_ = app_cf_.parsa();
  num_partitions_ = conf_.num_partitions();
  num_V_ = conf_.v_size();
  data_buf_.setMaxCapacity((size_t)conf_.data_buff_size_in_mb() * 1000000);
  neighbor_set_.resize(num_partitions_);
  for (int k = 0; k < num_partitions_; ++k) {
    neighbor_set_[k].assigned_V.resize(num_V_);
  }
}

void Parsa::run() {
  partition();
}

void Parsa::partition() {
  data_thr_ = unique_ptr<std::thread>(new std::thread(&Parsa::loadInputGraphPtr, this));
  data_thr_->detach();

  GraphPtrList X;
  SArray<int> map_U;
  for (int i = 0; ; ++i) {
    if (read_data_finished_ && data_buf_.empty()) break;
    data_buf_.pop(X);
    partitionU(X[0], X[1], &map_U);
  }
  partitionV();
}

void Parsa::loadInputGraphPtr() {
  conf_.mutable_input_graph()->set_ignore_feature_group(true);
  StreamReader<Empty> reader(searchFiles(conf_.input_graph()));
  MatrixPtrList<Empty> X;
  uint32 block_size = conf_.block_size();
  int i = 0;
  while(!read_data_finished_) {
    // load the graph
    bool ret = reader.readMatrices(block_size, &X);
    CHECK(X.size());
    auto G = std::static_pointer_cast<SparseMatrix<Key,Empty>>(X.back());

    // map the columns id into small integers
    size_t n = G->index().size(); CHECK_GT(n, 0);
    SArray<uint32> new_index(n);
    for (size_t i = 0; i < n; ++i) {
      new_index[i] = G->index()[i] % num_V_;
    }
    auto info = G->info();
    SizeR(0, num_V_).to(info.mutable_col());
    info.set_type(MatrixInfo::SPARSE_BINARY);
    info.set_sizeof_index(sizeof(uint32));

    GraphPtrList Y(2);
    Y[0] = GraphPtr(new Graph(info, G->offset(), new_index, SArray<Empty>()));
    Y[1] = std::static_pointer_cast<Graph>(Y[0]->toColMajor());
    data_buf_.push(Y, Y[0]->memSize() + Y[1]->memSize());

    LL << i ++;
    if (!ret) read_data_finished_ = true;
  }
}

void Parsa::partitionV() {
  std::vector<int> cost(num_partitions_);
  for (int i = 0; i < num_V_; ++i) {
    int max_v = 0;
    int max_j = -1;
    for (int j = 0; j < num_partitions_; ++j) {
      if (neighbor_set_[j].assigned_V[i]) {
        int v = ++ cost[j];
        if (v > max_v) { max_j = j; max_v = v; }
      }
    }
    /// assign V_j to partitionn max_j
    if (max_j >= 0) -- cost[max_j];
  }

  int v = 0;
  for (int j = 0; j < num_partitions_; ++j) v += cost[j];
  LL << v;
}

void Parsa::partitionU(
    const GraphPtr& row_major_blk, const GraphPtr& col_major_blk, SArray<int>* map_U) {
  // initialization
  int n = row_major_blk->rows();
  map_U->resize(n);
  assigned_U_.clear();
  assigned_U_.resize(n);
  initCost(row_major_blk);

  // partitioning
  for (int i = 0; i < n; ++i) {
    // TODO sync if necessary

    // assing U_i to partition k
    int k = i % num_partitions_;
    int Ui = cost_[k].minIdx();
    assigned_U_.set(Ui);
    (*map_U)[Ui] = k;

    // update
    updateCostAndNeighborSet(row_major_blk, col_major_blk, Ui, k);
  }
}

// init the cost of assigning U_i to partition k
void Parsa::initCost(const GraphPtr& row_major_blk) {
  int n = row_major_blk->rows();
  size_t* row_os = row_major_blk->offset().begin();
  uint32* row_idx = row_major_blk->index().begin();
  std::vector<int> cost(n);
  cost_.resize(num_partitions_);
  for (int k = 0; k < num_partitions_; ++k) {
    const auto& assigned_V = neighbor_set_[k].assigned_V;
    for (int i = 0; i < n; ++ i) {
      for (size_t j = row_os[i]; j < row_os[i+1]; ++j) {
       cost[i] += !assigned_V[row_idx[j]];
      }
    }
    cost_[k].init(cost, conf_.cost_cache_limit());
  }
}

void Parsa::updateCostAndNeighborSet(
    const GraphPtr& row_major_blk, const GraphPtr& col_major_blk, int Ui, int partition) {
  for (int s = 0; s < num_partitions_; ++s) cost_[s].remove(Ui);

  size_t* row_os = row_major_blk->offset().begin();
  size_t* col_os = col_major_blk->offset().begin();
  uint32* row_idx = row_major_blk->index().begin();
  uint32* col_idx = col_major_blk->index().begin();
  auto& assigned_V = neighbor_set_[partition].assigned_V;
  auto& cost = cost_[partition];
  for (size_t i = row_os[Ui]; i < row_os[Ui+1]; ++i) {
    int Vj = row_idx[i];
    if (assigned_V[Vj]) continue;
    assigned_V.set(Vj);
    for (size_t s = col_os[Vj]; s < col_os[Vj+1]; ++s) {
      int Uk = col_idx[s];
      if (assigned_U_[Uk]) continue;
      cost.decrAndSort(Uk);
    }
  }
}

} // namespace PS

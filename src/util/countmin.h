#pragma once

#define NDEBUG
#include <vector>
#include <algorithm>
#include <functional>
#include <cstdint>
#include <stdexcept>
#include <cstring>
#include <Eigen/Dense>
#include "proto/app.pb.h"
#include "util/MurmurHash3.h"
#include "base/shared_array.h"

// FIXME why both are 63?
static inline char floor_log2(uint64_t x)
{
    /* __builtin_clz is count of leading zeros in an integer */
    /* essentially computes ceil(log2(x)) */
    return (63 - __builtin_clzll(x));
}
static inline char floor_log2(uint32_t x)
{
    /* __builtin_clz is count of leading zeros in an integer */
    /* essentially computes ceil(log2(x)) */
    return (63 - __builtin_clzll(x));
}
static inline bool is_power_of_two(uint32_t x) {
    return (x & (x - 1)) == 0;
}

namespace PS
{

// FIXME i guess all files including this.h will use Eigen
// using namespace Eigen;
// using Eigen::Matrix;

template <class OBJ, class CNT>
class countmin_sketch {
    private:
  Eigen::Matrix<CNT, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> _arr; /* n by d array. d hash tables with length n */

//        shared_ptr<std::function<uint32_t (OBJ)> > _hash;
        // FIXME _n should be pow of 2? since you use &(n-1). if yes, add CHECK in ctor
        uint32_t _n; /* 'current' length of each hash table */
        const int _d; /* number of hash functions */

        //std::function<uint32_t (OBJ)> *hash() const { return _hash.get(); } ;

    public:
        countmin_sketch(const CountminConfig &cf) : _n(cf.table_size()), _d(cf.num_hash()) {
            CHECK_LE(cf.num_hash(), cf.seed_size());
            //auto *hashes = new std::function<uint32_t (OBJ)>[cf.num_hash()];
            //for (int i = 0; i < cf.num_hash(); i++)
            //{
                ///uint32_t seed = cf.seed(i);
//                hashes[i] = [seed](OBJ obj) {
//                                return obj;
//                            };
            //}
            //_hash.reset(hashes, [](std::function<uint32_t (OBJ)> *p) { delete [] p; });
            _arr.resize(_d, _n);
        }

        /*
         * Copy Constructor
         *
         * Creates object by copying _arr of original object
         *
         * orig : reference to object for copying
         *
         */
        explicit countmin_sketch(const countmin_sketch<OBJ, CNT> &orig);

        countmin_sketch(countmin_sketch<OBJ, CNT>&& other)
        : countmin_sketch(other._hash, other._d)
        {
            swap(*this, other);
        }

        /*
         * Destructor : frees _arr. No operation to _hash;
         *
         */
        ~countmin_sketch() {}

        /*
         * swap
         *
         *
         */
        friend void swap(countmin_sketch<OBJ,CNT> &first, countmin_sketch<OBJ,CNT> & second)
        {
            using std::swap;
            swap(first._n, second._n);
            first._hash.swap(second._hash);
            first._arr.swap(second._arr);
        }
        /*
         * insert: insert object into countmin sketch
         *
         */
        void insert(OBJ obj, CNT count);
        void insert(const SArray<OBJ> &key, const SArray<CNT> &count);
        void insert(std::vector<SArray<OBJ> > &hashes, const SArray<CNT> &count);
        void recoverFromBackup(std::vector<SArray<CNT> > &counts);
        /*
         * query: returns count of obj according to this sketch
         */
        CNT query(OBJ obj) const ;
        SArray<CNT> query(const SArray<OBJ> &key) const ;
        SArray<CNT> query(std::vector<SArray<OBJ> > &hashes) const ;

        CNT query(char d, OBJ obj) const  {
            return _arr.coeff(d, obj & (_n -1)) ;
            //return _arr.coeff(d, _hash[d](obj) & (_n -1));
        }

        CNT query(char d, uint32_t hash){
            return _arr.coeff(d, hash & (_n - 1));
        }
#ifdef DEBUG
        void dump(uint32_t d, uint32_t hash, uint32_t num_items) const;
#endif

        /* Queries for interpolating sketch */
        CNT query(char d, OBJ obj, size_t reduction_n) const;
  Eigen::Matrix<CNT, 1, Eigen::Dynamic> bulk_query(char d, OBJ obj, uint32_t n_item) const ;

        std::vector<SArray<CNT> > backup() const;

        void decrease_item_resolution_unsafe(uint32_t factor);
        /*
         * decrease_item_resolution:
         *
         * reduces size of hash table by factor.
         * ex) decrease_item resolution(2) --> reduces size by half
         *
         * Throws runtime exception if factor != powerof(2)
         */
        void decrease_item_resolution(uint32_t factor);
        /*
         * merge
         *
         * adds count of other sketch into this sketch
         *
         * Note: reduces this sketch or other sketch if this->_n != other._n
         *
         */
        void merge(const countmin_sketch<OBJ, CNT> &other);

        /*
         * get_n
         *
         * getter for _n
         */
        uint32_t get_n() const { return _n;}
        countmin_sketch<OBJ, CNT> &operator= (countmin_sketch<OBJ, CNT> other);
#ifdef DEBUG
        uint64_t total_count() const {return _total_count;}
#endif
};

template <class OBJ, class CNT>
countmin_sketch<OBJ, CNT> &countmin_sketch<OBJ, CNT>::operator=(countmin_sketch<OBJ, CNT> other)
{
    swap(*this, other);
    return *this;
}
template <class OBJ, class CNT>
void countmin_sketch<OBJ, CNT>::insert(OBJ obj, CNT count) {
    /* for each hash function */
    for (uint32_t i = 0; i < _d; i++) {
        //uint32_t temp = hash()[i](obj) & (_n-1) ;
        uint32_t temp = obj & (_n-1);
        /* computes the hash function and increment counter */
        //std::cout << "(" << i << "," << temp << ") into (" << _arr.rows() << "," << _arr.cols() << ")" << std::endl ;
        _arr.coeffRef(i, temp) += count;
    }
}

template <class OBJ, class CNT>
void countmin_sketch<OBJ, CNT>::insert(const SArray<OBJ> &keys, const SArray<CNT> &counts) {
    /* for each hash function */
    for (uint32_t i = 0; i < _d; i++) {
        #pragma omp parallel for num_threads(FLAGS_num_threads)
        for (size_t idx = 0; idx < keys.size(); idx++ )
        {
            uint32_t temp = keys[idx] & (_n - 1);
            size_t insert_loc = i * _n  + temp;
            __sync_fetch_and_add(_arr.data() + insert_loc, counts[idx]);
//            uint32_t temp = hash()[i](keys[idx]) & (_n-1) ;
//            _arr.coeffRef(i, temp) += counts[idx];
        }
    }
}

template <class OBJ, class CNT>
void countmin_sketch<OBJ, CNT>::insert(std::vector<SArray<OBJ> > &hashes, const SArray<CNT> &counts) {
  // FIXME do check, you assume a lot about the sizes
    for (uint32_t i = 0; i < _d; i++) {

        for (size_t idx = 0; idx < hashes[0].size(); idx++ )
        {
            _arr.coeffRef(i, hashes[i][idx] & (_n-1)) += counts[idx];
        }
    }
}

template <class OBJ, class CNT>
void countmin_sketch<OBJ, CNT>::recoverFromBackup(std::vector<SArray<CNT> > &counts) {
    CHECK_EQ(counts.size(), _d);
    CHECK_EQ(counts[0].size(), _n);
    for (size_t i = 0; i < _d; i++) {
        _arr.middleRows(i, 1) = counts[i].vec().transpose();
    }
}

template <class OBJ, class CNT>
CNT countmin_sketch<OBJ, CNT>::query(OBJ obj) const {
    /* return min of counters for each hash function */
    /* first assume counter according to first hash function is minimum */
    CNT min = _arr.coeff(0, obj * (_n - 1)); //_arr.coeff(0, hash()[0](obj) & (_n-1));
    for (uint32_t i = 1; i < _d; i++) {
        uint32_t temp = obj & (_n - 1); //hash()[i](obj) & (_n-1);
        /* counter according to hash function i */

        CNT val = _arr.coeff(i, temp);
        if (val < min) {
            /* new counter value is minimum value */
            min = val;
        }
    }
    return min;
}
template <class OBJ, class CNT>
SArray<CNT> countmin_sketch<OBJ, CNT>::query(const SArray<OBJ> &keys) const {
    SArray<CNT> counts(keys.size());
    /* return min of counters for each hash function */
    /* first assume counter according to first hash function is minimum */

    for (size_t idx = 0; idx < keys.size(); idx++)
    {
        counts[idx] = _arr.coeff(0, keys[idx] & (_n-1)); //_arr.coeff(0, hash()[0](keys[idx]) & (_n-1));
    }
    for (uint32_t i = 1; i < _d; i++) {
        for (size_t idx = 0; idx < keys.size(); idx++) {
            counts[idx] = std::min(counts[idx], _arr.coeff(i, keys[idx] & (_n-1)));
        }
    }

    return counts;
}

template <class OBJ, class CNT>
SArray<CNT> countmin_sketch<OBJ, CNT>::query(std::vector<SArray<OBJ> > &hashes) const {
    SArray<CNT> counts(hashes[0].size());
    /* return min of counters for each hash function */
    /* first assume counter according to first hash function is minimum */

    for (size_t idx = 0; idx < hashes[0].size(); idx++)
    {
        counts[idx] = _arr.coeff(0, hashes[0][idx] & (_n-1));
        //CHECK_NE(counts[idx],0);
    }
    for (uint32_t i = 1; i < _d; i++) {
        for (size_t idx = 0; idx < hashes[0].size(); idx++) {
            counts[idx] = std::min(counts[idx], _arr.coeff(i, hashes[i][idx] & (_n-1)));
            //CHECK_NE(counts[idx],0);
        }
    }

    return counts;
}

template <class OBJ, class CNT>
std::vector<SArray<CNT> > countmin_sketch<OBJ, CNT>::backup() const {
    std::vector<SArray<CNT> > backup;
    backup.reserve(_d);

    LL << _d;
    for (size_t i = 0; i < _d; i++) {
        SArray<CNT> row(_n);
        row.vec() = _arr.middleRows(i, 1).transpose();
        backup.push_back(row);
    }

    LL << backup.size();
    return backup;
}

#ifdef DEBUG
template <class OBJ, class CNT>
void countmin_sketch<OBJ, CNT>::dump(uint32_t d, uint32_t hash, uint32_t num_items) const {
    /* return min of counters for each hash function */
    size_t modulo = _n / num_items;
    std::cout << modulo << " * i + " << (hash&(modulo -1) ) << "\t";

    for (size_t i = 0; i < num_items; i++)
    {
        std::cout << "\t" << _arr.coeff(d, i * modulo + (hash & (modulo -1)));
    }
    std::cout << std::endl;
}
#endif
template <class OBJ, class CNT>
CNT countmin_sketch<OBJ, CNT>::query(char d, OBJ obj, size_t reduced_n) const {

    size_t n_item = _n >> floor_log2((uint64_t)reduced_n);
    uint32_t idx = obj & (reduced_n - 1); // _hash[(size_t)d](obj) & (reduced_n - 1);


    /* return min of counters for each hash function */
    CNT sum = 0;
    for (size_t i = 0; i < n_item; i++)
    {
        #ifdef DEBUG
            std::cout << "," << reduced_n * i + idx;
        #endif
        sum += _arr.coeff(d, reduced_n * i + idx);
    }
    #ifdef DEBUG
    std::cout << ")" << std::endl;
    #endif

    return sum;
}
template <class OBJ, class CNT>
    Eigen::Matrix<CNT, 1, Eigen::Dynamic> countmin_sketch<OBJ, CNT>::bulk_query(char d, OBJ obj, uint32_t n_item) const
{
    uint32_t n = _n >> floor_log2(n_item);

    uint32_t offset = obj & (n - 1);// _hash[0](obj) & (n - 1);
    Eigen::Matrix<CNT, 1, Eigen::Dynamic> val(1, n_item);

    for (size_t j = 0; j < n_item; j++)
    {
        val(j) = _arr.coeff(d, n * j + offset);
    }

    return val;

}

template <class OBJ, class CNT>
void countmin_sketch<OBJ, CNT>::merge(const countmin_sketch<OBJ, CNT> &other) {
#ifdef DEBUG
    if (_n != other._n)
    {

        std::cout << "n doesn't match: " << _n << " vs. " << other._n << std::endl;
        assert(0);
    }
#endif
    _arr += other._arr;
#ifdef DEBUG
    if (_arr.sum() != _total_count + other._total_count)
    {
        std::cout << "sum doesn't match: " << _arr.sum() << " vs. " << _total_count + other._total_count << std::endl;
        std::cout << "previously this sketch: " << _total_count << std::endl;
        std::cout << "previously other sketch: " << other._total_count << std::endl;
        assert(0);
    }
    _total_count += other._total_count;
#endif
}

template <class OBJ, class CNT>
void countmin_sketch<OBJ, CNT>::decrease_item_resolution_unsafe(uint32_t factor) {
    // Warning: this does not copy data at all. USe this at your own risk
    uint32_t new_n = _n >> floor_log2(factor);
    _arr.resize(Eigen::NoChange, new_n);
    _n = new_n;
}
template <class OBJ, class CNT>
void countmin_sketch<OBJ, CNT>::decrease_item_resolution(uint32_t factor) {
    //std::cout << "Enter here" << std::endl;


    if (_n == 1 || factor == 1)
        return;

    if (!is_power_of_two(factor)) {
        throw std::runtime_error("factor not eq power of two");
    }
    /* __builtin_clz returns # of leading zeros in a variable */
    /* effectively this is a ceil(log2(factor)) */

    uint32_t log2_factor = 31 - (uint32_t)__builtin_clz(factor);

    uint32_t new_n = _n >> log2_factor;
    for (uint32_t i = 1; i < factor; i ++)
    {
        _arr.leftCols(new_n) += _arr.middleCols(i * new_n, new_n);
    }
    /* copy completed. */
    _n = new_n;

    /* TODO: slow!!!! */
    _arr.conservativeResize(Eigen::NoChange, new_n);
#ifdef DEBUG
    if (_arr.sum() != _total_count)
    {
        std::cout << "reduced resolution sum doesn't match: " << _arr.sum() << " vs. " << _total_count<< std::endl;
        std::cout << "previously this sketch: " << _total_count << std::endl;
        assert(0);
    }
#endif
}

}

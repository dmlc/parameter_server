#pragma once
#include "proto/range.pb.h"

namespace PS {

template<class T> class Range;
typedef Range<size_t> SizeR;

// a range between [begin_, end_)
template<class T>
class Range {
 public:
  static Range all() { return Range(0,-1); }
  Range() : begin_(0), end_(0) { }

  template<typename V>
  Range(const Range<V>& other) { set(other.begin(), other.end()); }

  template<typename V, typename W>
  Range(V begin, W end) { set(begin, end); }

  Range(const PbRange& pb) { copyFrom(pb); }

  template <typename V>
  void operator=(const Range<V>& rhs) { set(rhs.begin(), rhs.end()); }

  // construct from a protobuf range
  void copyFrom(const PbRange& pb) { set(pb.begin(), pb.end()); }

  // fill a protobuf range
  void to(PbRange* pb) const { pb->set_begin(begin_); pb->set_end(end_); }

  template <typename V, typename W>
  void set(V start, W end) {
    begin_ = static_cast<T>(start);
    end_ = static_cast<T>(end);
  }

  T begin() const { return begin_; }
  T& begin() { return begin_; }
  T end() const { return end_; }
  T& end() { return end_; }

  size_t size() const { return (size_t)(end_ - begin_); }

  bool valid() const { return end_ >= begin_; }
  bool empty() const { return begin_ >= end_; }

  bool operator== (const Range& rhs) const {
    return (begin_ == rhs.begin_ && end_ == rhs.end_);
  }

  Range operator+ (const T v) const { return Range(begin_+v, end_+v); }
  Range operator- (const T v) const { return Range(begin_-v, end_-v); }
  Range operator* (const T v) const { return Range(begin_*v, end_*v); }

  template <typename V> bool contains(const V& v) const {
    return (begin_ <= static_cast<T>(v) && static_cast<T>(v) < end_);
  }

  Range setIntersection(const Range& dest) const {
    return Range(std::max(begin_, dest.begin_), std::min(end_, dest.end_));
  }

  Range setUnion(const Range& dest) const {
    return Range(std::min(begin_, dest.begin_), std::max(end_, dest.end_));
  }

  // divide this range evenly into n ones, and return the i-th
  Range evenDivide(size_t n, size_t i) const;

  std::string toString() const {
    return ("["+std::to_string(begin_)+","+std::to_string(end_)+")");
  }
 private:
  T begin_;
  T end_;
};


template<class T>
Range<T> Range<T>::evenDivide(size_t n, size_t i) const {
  CHECK(valid());
  CHECK_LT(i, n);
  auto itv = static_cast<long double>(end_ - begin_) /
             static_cast<long double>(n);
  return Range(static_cast<T>(begin_+itv*i), static_cast<T>(begin_+itv*(i+1)));
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const Range<T>& obj) {
  return (os << obj.toString());
}

} // namespace PS

namespace std {
template<typename T>
struct hash<PS::Range<T> > {
  std::size_t operator()(PS::Range<T> const& s) const {
    // return std::hash<std::pair<T,T> >()(std::make_pair(s.begin(), s.end()));
    // return (std::hash<T>(s.begin()) ^ (std::hash<T>(s.end()) << 1));
    return (size_t)(s.begin() ^ s.end() << 1);
  }
};
} // namespace std

#pragma once

namespace PS {

// a range between [start, end)
template<class T>
class Range {
 public:
  // static Range Invalid() { return Range(0,0); }
  static Range All() { return Range(1,0); }
  Range() : start_(0), end_(0) { }
  Range(T start, T end) : start_(start), end_(end) { }
  T start() const { return start_; }
  T end() const { return end_; }
  T& start() { return start_; }
  T& end() { return end_; }
  void Set(T start, T end) {
    start_ = start;
    end_ = end;
  }
  // return true if *this is contained by v
  // bool In(const T& v) { return (start_ >= v && v >= end_); }
  bool SubsetEq(const Range<T> v) {
    return (start_ >= v.start_ && v.end_ >= end_);
  }
  inline bool operator< (const Range& rhs) const;
  inline bool operator== (const Range& rhs) const;
  // return a key range limited by dest
  inline Range Limit(const Range& dest) const;
  // divide this range evenly into n ones, and return the i-th
  inline Range EvenDivide(size_t n, size_t i, T min_seg = 0) const;
  // do even divide if the segment if larger than min_seg, otherwise, all
  // segment will be zero except the 0-th
  bool Valid() const { return end_ > start_; }
  T Size() const { return end_ - start_; }
  T size() const { return end_ - start_; }
  string ToString() { return StrCat("[", std::to_string(start_),",",
                                    std::to_string(end_),"]"); }
 private:
  T start_;
  T end_;
};

template<class T>
bool Range<T>::operator< (const Range& rhs) const {
  bool ret = start_ < rhs.start_ || end_ < rhs.end_;
  return ret;
}

template<class T>
bool Range<T>::operator== (const Range& rhs) const {
  return (start_ == rhs.start_ && end_ == rhs.end_);
}

template<class T>
Range<T> Range<T>::Limit(const Range& dest) const {
  // T start = start_ < dest.start_ ? start_ : dest.start_;
  // T end = end_ < dest.end_ ? end_ : dest.end_;
  T start = std::max(start_, dest.start_);
  T end = std::min(end_, dest.end_);
  return Range(start, end);
}

template<class T>
Range<T> Range<T>::EvenDivide(size_t n, size_t i, T min_seg) const {
  CHECK_LT(i, n);
  if (end_ - start_ < min_seg) {
    if (i == 0)
      return *this;
    else
      return Range(end_, end_);
  }
  T itv = (end_ - start_) / n;
  T segment_start = std::max(start_ + itv*i, start_);
  // T segment_end = std::min(start_ + itv*(i+1), end_);
  T segment_end;
  if (i == n-1)
    segment_end = end_;
  else
    segment_end = std::min(start_ + itv*(i+1), end_);

  return Range(segment_start, segment_end);
}

} // namespace PS

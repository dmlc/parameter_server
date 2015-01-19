#pragma once
namespace PS {
namespace GP {

static const int kMaxNumPartitions = 16;
typedef uint8 P;  // support up to 256 partitions
typedef uint16 V; // support up to 16 partitions

typedef std::pair<Key, P> KP;

} // namespace GP
} // namespace PS

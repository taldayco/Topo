#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

// Simple param hash for cache invalidation
struct NoiseCache {
  enum Slot { ELEVATION = 0, RIVER = 1, WORLEY = 2, SLOT_COUNT = 3 };

  struct CacheEntry {
    uint64_t param_hash = 0;
    std::vector<float> data;
    std::vector<float> data2; // second output for Worley (edge)
    bool valid = false;
  };

  CacheEntry entries[SLOT_COUNT];

  // FNV-1a hash over raw bytes of a params struct
  template <typename T> static uint64_t hash_params(const T &params) {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&params);
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < sizeof(T); ++i) {
      hash ^= bytes[i];
      hash *= 1099511628211ULL;
    }
    return hash;
  }

  bool get(Slot slot, uint64_t param_hash, std::vector<float> &out) const {
    const auto &e = entries[slot];
    if (e.valid && e.param_hash == param_hash) {
      out = e.data;
      return true;
    }
    return false;
  }

  bool get2(Slot slot, uint64_t param_hash, std::vector<float> &out1,
            std::vector<float> &out2) const {
    const auto &e = entries[slot];
    if (e.valid && e.param_hash == param_hash) {
      out1 = e.data;
      out2 = e.data2;
      return true;
    }
    return false;
  }

  void put(Slot slot, uint64_t param_hash, const std::vector<float> &data) {
    auto &e = entries[slot];
    e.param_hash = param_hash;
    e.data = data;
    e.valid = true;
  }

  void put2(Slot slot, uint64_t param_hash, const std::vector<float> &data1,
            const std::vector<float> &data2) {
    auto &e = entries[slot];
    e.param_hash = param_hash;
    e.data = data1;
    e.data2 = data2;
    e.valid = true;
  }

  void invalidate_all() {
    for (auto &e : entries)
      e.valid = false;
  }
};

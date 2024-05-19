#pragma once

#include <string>
#include <vector>

#include "rocksdb/cleanable.h"
#include "rocksdb/slice.h"
#include "rocksdb/status.h"

namespace ROCKSDB_NAMESPACE {


extern uint8_t max_n_past_timestamps;
extern uint8_t max_n_past_distances;
// extern const uint8_t n_edc_feature;
extern  uint8_t n_edc_feature;
extern std::vector<double> hash_edc;
extern uint32_t max_hash_edc_idx;
extern uint32_t memory_window;
extern uint8_t base_edc_window;
extern std::vector<uint32_t> edc_windows;



class MetaExtra {
public:
    //vector overhead = 24 (8 pointer, 8 size, 8 allocation)
    //40 + 24 + 4*x + 1
    //65 byte at least
    //189 byte at most
    //not 1 hit wonder
    float _edc[10];
    std::vector<uint32_t> _past_distances;
    //the next index to put the distance
    uint8_t _past_distance_idx = 1;

    MetaExtra(const uint32_t &distance) {
        _past_distances = std::vector<uint32_t>(1, distance);
        for (uint8_t i = 0; i < n_edc_feature; ++i) {
            uint32_t _distance_idx = std::min(uint32_t(distance / edc_windows[i]), max_hash_edc_idx);
            _edc[i] = hash_edc[_distance_idx] + 1;
        }
    }

    void update(const uint32_t &distance) {
        uint8_t distance_idx = _past_distance_idx % max_n_past_distances;
        if (_past_distances.size() < max_n_past_distances)
            _past_distances.emplace_back(distance);
        else
            _past_distances[distance_idx] = distance;
        assert(_past_distances.size() <= max_n_past_distances);
        _past_distance_idx = _past_distance_idx + (uint8_t) 1;
        if (_past_distance_idx >= max_n_past_distances * 2)
            _past_distance_idx -= max_n_past_distances;
        for (uint8_t i = 0; i < n_edc_feature; ++i) {
            uint32_t _distance_idx = std::min(uint32_t(distance / edc_windows[i]), max_hash_edc_idx);
            _edc[i] = _edc[i] * hash_edc[_distance_idx] + 1;
        }
    }
private:
};
class KeyMeta {
public:
  // used as timestamp
  uint64_t past_sequence_;
  uint64_t size_;
  uint64_t past_sequqence_2_;
  bool is_sampled_ = false;

  MetaExtra *_extra = nullptr;
  std::vector<uint32_t> _sample_times;

  KeyMeta(const uint64_t& _past_sequence, uint64_t size)
    : past_sequence_(_past_sequence),
      size_(size) {
    past_sequqence_2_ = 0;
  }

  ~KeyMeta() {
    if(_extra != nullptr)
        delete _extra;
  }

  void SetSampled() {
    is_sampled_ = true;
  }
  void UnsetSampled() {
    is_sampled_ = false;
  }
  bool IsSampled() const {
    return is_sampled_;
  }

  uint64_t GetLabel() const {
    if(_extra == nullptr)
        return 0;
    return past_sequence_ - past_sequqence_2_;
  }

  // use arena?
  void Update(const uint64_t& _past_sequence) {
    uint64_t distance = _past_sequence - past_sequence_;
    assert(distance > 0);
    if(_extra == nullptr)
        _extra = new MetaExtra(distance);
    else
        _extra->update(distance);
    past_sequqence_2_ = past_sequence_;
    past_sequence_ = _past_sequence;
  }
};


}

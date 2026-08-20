#pragma once

#include <cstddef>
#include <functional>
#include <list>
#include <utility>
#include <vector>

namespace mini_redis::bench {

template <class Key, class Value, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>>
class SynchronousHashTable final {
 public:
  explicit SynchronousHashTable(
      std::size_t bucket_count = 8,
      float max_load_factor = 0.75F)
      : buckets_(bucket_count == 0 ? 1 : bucket_count),
        max_load_factor_(max_load_factor) {}

  bool insert_or_assign(Key key, Value value) {
    if (Entry* existing = find_entry(key); existing != nullptr) {
      existing->value = std::move(value);
      return false;
    }
    if (static_cast<float>(size_ + 1) >
        static_cast<float>(buckets_.size()) * max_load_factor_) {
      rehash(buckets_.size() * 2);
    }
    Bucket& bucket =
        buckets_[bucket_index(key, buckets_.size())];
    bucket.push_back(Entry{std::move(key), std::move(value)});
    ++size_;
    return true;
  }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }

 private:
  struct Entry final {
    Key key;
    Value value;
  };
  using Bucket = std::list<Entry>;

  [[nodiscard]] std::size_t bucket_index(
      const Key& key, std::size_t count) const {
    return hash_(key) % count;
  }

  [[nodiscard]] Entry* find_entry(const Key& key) {
    Bucket& bucket = buckets_[bucket_index(key, buckets_.size())];
    for (Entry& entry : bucket) {
      if (key_equal_(entry.key, key)) {
        return &entry;
      }
    }
    return nullptr;
  }

  void rehash(std::size_t count) {
    std::vector<Bucket> replacement(count);
    for (Bucket& bucket : buckets_) {
      while (!bucket.empty()) {
        const std::size_t destination =
            bucket_index(bucket.front().key, count);
        replacement[destination].splice(
            replacement[destination].end(), bucket, bucket.begin());
      }
    }
    buckets_.swap(replacement);
  }

  std::vector<Bucket> buckets_;
  std::size_t size_{0};
  float max_load_factor_;
  Hash hash_;
  KeyEqual key_equal_;
};

}  // namespace mini_redis::bench

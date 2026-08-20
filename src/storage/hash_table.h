#pragma once

#include <cstddef>
#include <functional>
#include <limits>
#include <list>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mini_redis {

template <class Key, class Value, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>>
class HashTable final {
 public:
  explicit HashTable(std::size_t bucket_count = 8,
                     float max_load_factor = 0.75F, Hash hash = {},
                     KeyEqual key_equal = {})
      : buckets_(normalize_bucket_count(bucket_count)),
        max_load_factor_(max_load_factor),
        hash_(std::move(hash)),
        key_equal_(std::move(key_equal)) {
    if (max_load_factor <= 0.0F) {
      throw std::invalid_argument("max load factor must be positive");
    }
  }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
  [[nodiscard]] std::size_t bucket_count() const noexcept {
    return is_rehashing() ? new_buckets_.size() : buckets_.size();
  }
  [[nodiscard]] float load_factor() const noexcept {
    return static_cast<float>(size_) /
           static_cast<float>(bucket_count());
  }
  [[nodiscard]] float max_load_factor() const noexcept {
    return max_load_factor_;
  }
  [[nodiscard]] bool is_rehashing() const noexcept {
    return rehash_index_ != kNotRehashing;
  }
  [[nodiscard]] float rehash_progress() const noexcept {
    if (!is_rehashing()) {
      return 1.0F;
    }
    return static_cast<float>(rehash_index_) /
           static_cast<float>(buckets_.size());
  }

  bool insert_or_assign(Key key, Value value) {
    rehash_step();

    if (Entry* existing = find_entry(key); existing != nullptr) {
      existing->value = std::move(value);
      return false;
    }

    if (!is_rehashing() &&
        static_cast<float>(size_ + 1) >
            static_cast<float>(buckets_.size()) *
                max_load_factor_) {
      start_rehash(buckets_.size() * 2);
    }

    Table& destination =
        is_rehashing() ? new_buckets_ : buckets_;
    Bucket& bucket =
        destination[bucket_index(key, destination.size())];
    bucket.push_back(Entry{std::move(key), std::move(value)});
    ++size_;
    return true;
  }

  [[nodiscard]] Value* find(const Key& key) {
    Entry* existing = find_entry(key);
    return existing == nullptr ? nullptr : &existing->value;
  }
  [[nodiscard]] const Value* find(const Key& key) const {
    const Entry* existing = find_entry(key);
    return existing == nullptr ? nullptr : &existing->value;
  }
  [[nodiscard]] bool contains(const Key& key) const {
    return find_entry(key) != nullptr;
  }

  bool erase(const Key& key) {
    rehash_step();

    if (is_rehashing() && erase_from(new_buckets_, key)) {
      --size_;
      return true;
    }
    if (erase_from(buckets_, key)) {
      --size_;
      return true;
    }
    return false;
  }

  void clear() noexcept {
    for (Bucket& bucket : buckets_) {
      bucket.clear();
    }
    for (Bucket& bucket : new_buckets_) {
      bucket.clear();
    }
    if (is_rehashing()) {
      buckets_.swap(new_buckets_);
      new_buckets_.clear();
      rehash_index_ = kNotRehashing;
    }
    size_ = 0;
  }

  void reserve(std::size_t element_count) {
    rehash(required_bucket_count(element_count));
  }

  void rehash(std::size_t requested_bucket_count) {
    if (is_rehashing()) {
      finish_rehash();
    }

    const std::size_t minimum_bucket_count =
        required_bucket_count(size_);
    const std::size_t new_bucket_count = normalize_bucket_count(
        requested_bucket_count < minimum_bucket_count
            ? minimum_bucket_count
            : requested_bucket_count);
    start_rehash(new_bucket_count);
  }

 private:
  struct Entry final {
    Key key;
    Value value;
  };

  using Bucket = std::list<Entry>;
  using Table = std::vector<Bucket>;

  static constexpr std::size_t kNotRehashing =
      std::numeric_limits<std::size_t>::max();

  [[nodiscard]] static std::size_t normalize_bucket_count(
      std::size_t bucket_count) noexcept {
    return bucket_count == 0 ? 1 : bucket_count;
  }

  [[nodiscard]] std::size_t bucket_index(
      const Key& key, std::size_t bucket_count) const {
    return hash_(key) % bucket_count;
  }

  [[nodiscard]] std::size_t required_bucket_count(
      std::size_t element_count) const noexcept {
    std::size_t result = static_cast<std::size_t>(
        static_cast<float>(element_count) / max_load_factor_);
    if (static_cast<float>(result) * max_load_factor_ <
        static_cast<float>(element_count)) {
      ++result;
    }
    return normalize_bucket_count(result);
  }

  void start_rehash(std::size_t new_bucket_count) {
    if (new_bucket_count == buckets_.size()) {
      return;
    }
    new_buckets_ = Table(new_bucket_count);
    rehash_index_ = 0;
  }

  void rehash_step() {
    if (!is_rehashing()) {
      return;
    }

    Bucket& old_bucket = buckets_[rehash_index_];
    while (!old_bucket.empty()) {
      const std::size_t destination =
          bucket_index(old_bucket.front().key, new_buckets_.size());
      new_buckets_[destination].splice(
          new_buckets_[destination].end(), old_bucket,
          old_bucket.begin());
    }

    ++rehash_index_;
    if (rehash_index_ == buckets_.size()) {
      buckets_.swap(new_buckets_);
      new_buckets_.clear();
      rehash_index_ = kNotRehashing;
    }
  }

  void finish_rehash() {
    while (is_rehashing()) {
      rehash_step();
    }
  }

  [[nodiscard]] Entry* find_in(Table& table, const Key& key) {
    Bucket& bucket = table[bucket_index(key, table.size())];
    for (Entry& entry : bucket) {
      if (key_equal_(entry.key, key)) {
        return &entry;
      }
    }
    return nullptr;
  }

  [[nodiscard]] const Entry* find_in(
      const Table& table, const Key& key) const {
    const Bucket& bucket =
        table[bucket_index(key, table.size())];
    for (const Entry& entry : bucket) {
      if (key_equal_(entry.key, key)) {
        return &entry;
      }
    }
    return nullptr;
  }

  [[nodiscard]] Entry* find_entry(const Key& key) {
    if (is_rehashing()) {
      if (Entry* existing = find_in(new_buckets_, key);
          existing != nullptr) {
        return existing;
      }
    }
    return find_in(buckets_, key);
  }

  [[nodiscard]] const Entry* find_entry(const Key& key) const {
    if (is_rehashing()) {
      if (const Entry* existing = find_in(new_buckets_, key);
          existing != nullptr) {
        return existing;
      }
    }
    return find_in(buckets_, key);
  }

  bool erase_from(Table& table, const Key& key) {
    Bucket& bucket = table[bucket_index(key, table.size())];
    for (auto entry = bucket.begin(); entry != bucket.end(); ++entry) {
      if (key_equal_(entry->key, key)) {
        bucket.erase(entry);
        return true;
      }
    }
    return false;
  }

  Table buckets_;
  Table new_buckets_;
  std::size_t rehash_index_{kNotRehashing};
  std::size_t size_{0};
  float max_load_factor_;
  Hash hash_;
  KeyEqual key_equal_;
};

}  // namespace mini_redis

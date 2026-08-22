#pragma once

#include "storage/hash_table.h"
#include "storage/skip_list.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mini_redis {

class ZSet final {
 public:
  using Entry = SkipList::Entry;

  enum class AddResult {
    kAdded,
    kUpdated,
    kUnchanged,
    kInvalidScore,
  };

  ZSet();
  explicit ZSet(std::uint32_t skip_list_seed);

  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;

  AddResult add(double score, std::string member);
  bool remove(std::string_view member);

  [[nodiscard]] std::optional<double> score_of(
      std::string_view member) const;

  [[nodiscard]] std::optional<std::size_t> rank_of(
      std::string_view member) const;

  [[nodiscard]] std::vector<Entry> range_by_rank(
      std::size_t start, std::size_t stop) const;

 private:
  HashTable<std::string, double> scores_;
  SkipList ordered_;
};

}  // namespace mini_redis

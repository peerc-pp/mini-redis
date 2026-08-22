#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace mini_redis {

class SkipList final {
 public:
  struct Entry final {
    double score;
    std::string member;

    [[nodiscard]] bool operator==(const Entry& other) const noexcept {
      return score == other.score && member == other.member;
    }
  };

  SkipList();
  explicit SkipList(std::uint32_t seed);
  ~SkipList();

  SkipList(const SkipList&) = delete;
  SkipList& operator=(const SkipList&) = delete;
  SkipList(SkipList&&) = delete;
  SkipList& operator=(SkipList&&) = delete;

  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;

  bool insert(double score, std::string member);
  bool erase(double score, std::string_view member);

  [[nodiscard]] std::optional<std::size_t> rank(
      double score, std::string_view member) const;

  [[nodiscard]] std::vector<Entry> range_by_rank(
      std::size_t start, std::size_t stop) const;

 private:
  struct Node;

  static constexpr std::size_t kMaxLevel = 32;

  [[nodiscard]] static bool less(
      double left_score, std::string_view left_member,
      double right_score, std::string_view right_member) noexcept;

  [[nodiscard]] static bool equal(
      const Node& node, double score,
      std::string_view member) noexcept;

  [[nodiscard]] std::size_t random_level();
  void clear() noexcept;

  std::unique_ptr<Node> head_;
  std::size_t level_count_{1};
  std::size_t size_{0};
  std::mt19937 generator_;
};

}  // namespace mini_redis

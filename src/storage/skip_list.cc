#include "storage/skip_list.h"

#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace mini_redis {

struct SkipList::Node final {
  struct Level final {
    Node* forward{nullptr};
    std::size_t span{0};
  };

  Node(double node_score, std::string node_member,
       std::size_t height)
      : score(node_score),
        member(std::move(node_member)),
        levels(height) {}

  double score;
  std::string member;
  std::vector<Level> levels;
};

SkipList::SkipList()
    : SkipList(std::random_device{}()) {}

SkipList::SkipList(std::uint32_t seed)
    : head_(std::make_unique<Node>(0.0, "", kMaxLevel)),
      generator_(seed) {}

SkipList::~SkipList() { clear(); }

std::size_t SkipList::size() const noexcept { return size_; }

bool SkipList::empty() const noexcept { return size_ == 0; }

bool SkipList::less(double left_score,
                    std::string_view left_member,
                    double right_score,
                    std::string_view right_member) noexcept {
  return left_score < right_score ||
         (left_score == right_score &&
          left_member < right_member);
}

bool SkipList::equal(const Node& node, double score,
                     std::string_view member) noexcept {
  return node.score == score && node.member == member;
}

std::size_t SkipList::random_level() {
  std::size_t level = 1;
  constexpr std::uint32_t kPromotionThreshold =
      std::numeric_limits<std::uint32_t>::max() / 4U;
  while (level < kMaxLevel &&
         generator_() <= kPromotionThreshold) {
    ++level;
  }
  return level;
}

bool SkipList::insert(double score, std::string member) {
  if (std::isnan(score)) {
    return false;
  }

  std::array<Node*, kMaxLevel> update{};
  std::array<std::size_t, kMaxLevel> ranks{};
  Node* current = head_.get();

  for (std::size_t level = level_count_; level-- > 0;) {
    ranks[level] = level + 1 == level_count_
                       ? 0
                       : ranks[level + 1];
    while (current->levels[level].forward != nullptr &&
           less(current->levels[level].forward->score,
                current->levels[level].forward->member,
                score, member)) {
      ranks[level] += current->levels[level].span;
      current = current->levels[level].forward;
    }
    update[level] = current;
  }

  current = current->levels[0].forward;
  if (current != nullptr && equal(*current, score, member)) {
    return false;
  }

  const std::size_t height = random_level();
  if (height > level_count_) {
    for (std::size_t level = level_count_; level < height;
         ++level) {
      ranks[level] = 0;
      update[level] = head_.get();
      head_->levels[level].span = size_;
    }
    level_count_ = height;
  }

  auto node = std::make_unique<Node>(
      score, std::move(member), height);
  Node* inserted = node.get();

  for (std::size_t level = 0; level < height; ++level) {
    inserted->levels[level].forward =
        update[level]->levels[level].forward;
    update[level]->levels[level].forward = inserted;

    const std::size_t skipped = ranks[0] - ranks[level];
    inserted->levels[level].span =
        update[level]->levels[level].span - skipped;
    update[level]->levels[level].span = skipped + 1;
  }

  for (std::size_t level = height; level < level_count_;
       ++level) {
    ++update[level]->levels[level].span;
  }

  ++size_;
  static_cast<void>(node.release());
  return true;
}

bool SkipList::erase(double score, std::string_view member) {
  if (std::isnan(score)) {
    return false;
  }

  std::array<Node*, kMaxLevel> update{};
  Node* current = head_.get();

  for (std::size_t level = level_count_; level-- > 0;) {
    while (current->levels[level].forward != nullptr &&
           less(current->levels[level].forward->score,
                current->levels[level].forward->member,
                score, member)) {
      current = current->levels[level].forward;
    }
    update[level] = current;
  }

  current = current->levels[0].forward;
  if (current == nullptr || !equal(*current, score, member)) {
    return false;
  }

  for (std::size_t level = 0; level < level_count_; ++level) {
    if (update[level]->levels[level].forward == current) {
      update[level]->levels[level].span +=
          current->levels[level].span - 1;
      update[level]->levels[level].forward =
          current->levels[level].forward;
    } else {
      --update[level]->levels[level].span;
    }
  }

  delete current;
  --size_;

  while (level_count_ > 1 &&
         head_->levels[level_count_ - 1].forward == nullptr) {
    --level_count_;
  }
  return true;
}

std::optional<std::size_t> SkipList::rank(
    double score, std::string_view member) const {
  if (std::isnan(score)) {
    return std::nullopt;
  }

  std::size_t traversed = 0;
  const Node* current = head_.get();

  for (std::size_t level = level_count_; level-- > 0;) {
    while (current->levels[level].forward != nullptr &&
           !less(score, member,
                 current->levels[level].forward->score,
                 current->levels[level].forward->member)) {
      traversed += current->levels[level].span;
      current = current->levels[level].forward;
      if (equal(*current, score, member)) {
        return traversed - 1;
      }
    }
  }
  return std::nullopt;
}

std::vector<SkipList::Entry> SkipList::range_by_rank(
    std::size_t start, std::size_t stop) const {
  std::vector<Entry> result;
  if (start > stop || start >= size_) {
    return result;
  }

  stop = stop < size_ ? stop : size_ - 1;
  result.reserve(stop - start + 1);

  const std::size_t target = start + 1;
  std::size_t traversed = 0;
  const Node* current = head_.get();
  for (std::size_t level = level_count_; level-- > 0;) {
    while (current->levels[level].forward != nullptr &&
           traversed + current->levels[level].span <= target) {
      traversed += current->levels[level].span;
      current = current->levels[level].forward;
    }
  }

  for (std::size_t index = start;
       current != nullptr && index <= stop; ++index) {
    result.push_back(Entry{current->score, current->member});
    current = current->levels[0].forward;
  }
  return result;
}

void SkipList::clear() noexcept {
  Node* current = head_->levels[0].forward;
  while (current != nullptr) {
    Node* next = current->levels[0].forward;
    delete current;
    current = next;
  }
}

}  // namespace mini_redis

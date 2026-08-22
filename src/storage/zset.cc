#include "storage/zset.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace mini_redis {

ZSet::ZSet() = default;

ZSet::ZSet(std::uint32_t skip_list_seed)
    : ordered_(skip_list_seed) {}

std::size_t ZSet::size() const noexcept { return scores_.size(); }

bool ZSet::empty() const noexcept { return scores_.empty(); }

ZSet::AddResult ZSet::add(double score, std::string member) {
  if (std::isnan(score)) {
    return AddResult::kInvalidScore;
  }

  double* existing_score = scores_.find(member);
  if (existing_score != nullptr) {
    if (*existing_score == score) {
      return AddResult::kUnchanged;
    }

    if (!ordered_.insert(score, member)) {
      throw std::logic_error(
          "ZSet ordered index rejected a new score");
    }

    const double old_score = *existing_score;
    if (!ordered_.erase(old_score, member)) {
      static_cast<void>(ordered_.erase(score, member));
      throw std::logic_error(
          "ZSet indexes disagree about an existing member");
    }

    *existing_score = score;
    return AddResult::kUpdated;
  }

  if (!ordered_.insert(score, member)) {
    throw std::logic_error(
        "ZSet ordered index contains an untracked member");
  }

  try {
    if (!scores_.insert_or_assign(member, score)) {
      static_cast<void>(ordered_.erase(score, member));
      throw std::logic_error(
          "ZSet member index changed during insertion");
    }
  } catch (...) {
    static_cast<void>(ordered_.erase(score, member));
    throw;
  }

  return AddResult::kAdded;
}

bool ZSet::remove(std::string_view member) {
  double* existing_score = scores_.find(std::string(member));
  if (existing_score == nullptr) {
    return false;
  }

  const double score = *existing_score;
  if (!ordered_.erase(score, member)) {
    throw std::logic_error(
        "ZSet indexes disagree about an existing member");
  }

  if (!scores_.erase(std::string(member))) {
    throw std::logic_error(
        "ZSet member index lost an existing member");
  }
  return true;
}

std::optional<double> ZSet::score_of(
    std::string_view member) const {
  const double* score = scores_.find(std::string(member));
  if (score == nullptr) {
    return std::nullopt;
  }
  return *score;
}

std::optional<std::size_t> ZSet::rank_of(
    std::string_view member) const {
  const std::optional<double> score = score_of(member);
  if (!score.has_value()) {
    return std::nullopt;
  }

  const std::optional<std::size_t> rank =
      ordered_.rank(*score, member);
  if (!rank.has_value()) {
    throw std::logic_error(
        "ZSet indexes disagree about an existing member");
  }
  return rank;
}

std::vector<ZSet::Entry> ZSet::range_by_rank(
    std::size_t start, std::size_t stop) const {
  return ordered_.range_by_rank(start, stop);
}

}  // namespace mini_redis

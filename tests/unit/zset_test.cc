#include "storage/zset.h"

#include <cstddef>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using AddResult = mini_redis::ZSet::AddResult;
using Entry = mini_redis::ZSet::Entry;
using Scores = std::unordered_map<std::string, double>;
using Ordered = std::set<std::pair<double, std::string>>;

bool test_add_update_and_equal_scores() {
  mini_redis::ZSet zset(0x5EEDU);

  if (zset.add(100.0, "carol") != AddResult::kAdded ||
      zset.add(80.0, "bob") != AddResult::kAdded ||
      zset.add(100.0, "alice") != AddResult::kAdded ||
      zset.add(100.0, "alice") != AddResult::kUnchanged) {
    return false;
  }

  if (zset.range_by_rank(0, 2) !=
      std::vector<Entry>{{80.0, "bob"},
                         {100.0, "alice"},
                         {100.0, "carol"}}) {
    return false;
  }

  return zset.add(130.0, "alice") == AddResult::kUpdated &&
         zset.score_of("alice") == 130.0 &&
         zset.rank_of("alice") == 2 &&
         zset.range_by_rank(0, 2) ==
             std::vector<Entry>{{80.0, "bob"},
                                {100.0, "carol"},
                                {130.0, "alice"}};
}

bool test_remove_and_invalid_score() {
  mini_redis::ZSet zset(0x5EEDU);
  zset.add(80.0, "bob");
  zset.add(100.0, "alice");

  const double nan = std::numeric_limits<double>::quiet_NaN();
  return zset.add(nan, "invalid") ==
             AddResult::kInvalidScore &&
         !zset.score_of("invalid").has_value() &&
         zset.remove("alice") && !zset.remove("alice") &&
         !zset.score_of("alice").has_value() &&
         !zset.rank_of("alice").has_value() &&
         zset.range_by_rank(0, 10) ==
             std::vector<Entry>{{80.0, "bob"}};
}

bool matches(const mini_redis::ZSet& actual,
             const Scores& scores,
             const Ordered& ordered) {
  if (actual.size() != scores.size() ||
      actual.size() != ordered.size()) {
    return false;
  }

  std::vector<Entry> expected_range;
  expected_range.reserve(ordered.size());
  for (const auto& [score, member] : ordered) {
    expected_range.push_back(Entry{score, member});
  }

  const std::vector<Entry> actual_range = actual.empty()
      ? std::vector<Entry>{}
      : actual.range_by_rank(0, actual.size() - 1);
  if (actual_range != expected_range) {
    return false;
  }

  std::size_t expected_rank = 0;
  for (const auto& [score, member] : ordered) {
    if (actual.score_of(member) != score ||
        actual.rank_of(member) != expected_rank) {
      return false;
    }
    ++expected_rank;
  }
  return true;
}

bool test_random_dual_index_operations() {
  mini_redis::ZSet actual(0x5EEDU);
  Scores scores;
  Ordered ordered;
  std::mt19937 generator(0xC0FFEEU);
  std::uniform_int_distribution<int> operation(0, 2);
  std::uniform_int_distribution<int> score(-100, 100);
  std::uniform_int_distribution<int> member(0, 200);

  for (int step = 0; step < 100000; ++step) {
    const std::string current_member =
        "member-" + std::to_string(member(generator));

    if (operation(generator) < 2) {
      const double current_score =
          static_cast<double>(score(generator));
      const auto existing = scores.find(current_member);
      AddResult expected_result = AddResult::kAdded;
      if (existing != scores.end()) {
        if (existing->second == current_score) {
          expected_result = AddResult::kUnchanged;
        } else {
          expected_result = AddResult::kUpdated;
          ordered.erase(
              std::make_pair(existing->second, current_member));
        }
      }

      if (actual.add(current_score, current_member) !=
          expected_result) {
        return false;
      }
      scores.insert_or_assign(current_member, current_score);
      ordered.insert(
          std::make_pair(current_score, current_member));
    } else {
      const auto existing = scores.find(current_member);
      const bool expected_removed = existing != scores.end();
      if (actual.remove(current_member) != expected_removed) {
        return false;
      }
      if (existing != scores.end()) {
        ordered.erase(
            std::make_pair(existing->second, current_member));
        scores.erase(existing);
      }
    }

    if (step % 100 == 0 &&
        !matches(actual, scores, ordered)) {
      return false;
    }
  }
  return matches(actual, scores, ordered);
}

bool run_test(const char* name, bool (*test)()) {
  if (test()) {
    return true;
  }
  std::cerr << name << " failed\n";
  return false;
}

}  // namespace

int main() {
  return run_test("add/update/equal score",
                  test_add_update_and_equal_scores) &&
                 run_test("remove/invalid score",
                          test_remove_and_invalid_score) &&
                 run_test("random dual-index operations",
                          test_random_dual_index_operations)
             ? 0
             : 1;
}

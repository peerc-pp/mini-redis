#include "storage/skip_list.h"

#include <cstddef>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

using Entry = mini_redis::SkipList::Entry;
using Expected = std::set<std::pair<double, std::string>>;

bool test_orders_by_score_then_member() {
  mini_redis::SkipList list(0x5EEDU);
  return list.insert(100.0, "carol") &&
         list.insert(80.0, "bob") &&
         list.insert(100.0, "alice") &&
         !list.insert(100.0, "alice") &&
         list.range_by_rank(0, 2) ==
             std::vector<Entry>{{80.0, "bob"},
                                {100.0, "alice"},
                                {100.0, "carol"}};
}

bool test_rank_and_erase() {
  mini_redis::SkipList list(0x5EEDU);
  list.insert(80.0, "bob");
  list.insert(100.0, "alice");
  list.insert(100.0, "carol");
  list.insert(120.0, "dave");

  const auto alice_rank = list.rank(100.0, "alice");
  const auto carol_rank = list.rank(100.0, "carol");
  if (!alice_rank.has_value() || *alice_rank != 1 ||
      !carol_rank.has_value() || *carol_rank != 2 ||
      list.rank(99.0, "alice").has_value()) {
    return false;
  }

  return list.erase(100.0, "alice") &&
         !list.erase(100.0, "alice") && list.size() == 3 &&
         list.rank(100.0, "carol") == 1 &&
         list.range_by_rank(0, 10) ==
             std::vector<Entry>{{80.0, "bob"},
                                {100.0, "carol"},
                                {120.0, "dave"}};
}

bool test_range_boundaries_and_nan() {
  mini_redis::SkipList list(0x5EEDU);
  for (int value = 0; value < 5; ++value) {
    list.insert(static_cast<double>(value),
                std::to_string(value));
  }

  const double nan = std::numeric_limits<double>::quiet_NaN();
  return list.range_by_rank(1, 3) ==
             std::vector<Entry>{{1.0, "1"},
                                {2.0, "2"},
                                {3.0, "3"}} &&
         list.range_by_rank(4, 3).empty() &&
         list.range_by_rank(5, 10).empty() &&
         !list.insert(nan, "nan") &&
         !list.erase(nan, "nan") &&
         !list.rank(nan, "nan").has_value();
}

bool matches(const mini_redis::SkipList& actual,
             const Expected& expected) {
  if (actual.size() != expected.size()) {
    return false;
  }

  std::vector<Entry> expected_entries;
  expected_entries.reserve(expected.size());
  for (const auto& [score, member] : expected) {
    expected_entries.push_back(Entry{score, member});
  }

  const std::vector<Entry> actual_entries = actual.empty()
      ? std::vector<Entry>{}
      : actual.range_by_rank(0, actual.size() - 1);
  if (actual_entries != expected_entries) {
    return false;
  }

  std::size_t expected_rank = 0;
  for (const auto& [score, member] : expected) {
    if (actual.rank(score, member) != expected_rank) {
      return false;
    }
    ++expected_rank;
  }
  return true;
}

bool test_random_operations() {
  mini_redis::SkipList actual(0x5EEDU);
  Expected expected;
  std::mt19937 generator(0xC0FFEEU);
  std::uniform_int_distribution<int> operation(0, 1);
  std::uniform_int_distribution<int> score(-50, 50);
  std::uniform_int_distribution<int> member(0, 100);

  for (int step = 0; step < 10000; ++step) {
    const double current_score =
        static_cast<double>(score(generator));
    const std::string current_member =
        "member-" + std::to_string(member(generator));
    const auto value =
        std::make_pair(current_score, current_member);

    if (operation(generator) == 0) {
      if (actual.insert(current_score, current_member) !=
          expected.insert(value).second) {
        return false;
      }
    } else if (actual.erase(current_score, current_member) !=
               (expected.erase(value) != 0)) {
      return false;
    }

    if (step % 100 == 0 && !matches(actual, expected)) {
      return false;
    }
  }
  return matches(actual, expected);
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
  return run_test("ordering", test_orders_by_score_then_member) &&
                 run_test("rank/erase", test_rank_and_erase) &&
                 run_test("range/NaN",
                          test_range_boundaries_and_nan) &&
                 run_test("random operations",
                          test_random_operations)
             ? 0
             : 1;
}

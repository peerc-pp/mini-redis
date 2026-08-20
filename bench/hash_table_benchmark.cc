#include "storage/hash_table.h"

#include "synchronous_hash_table.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using Nanoseconds = std::chrono::nanoseconds;

struct Summary final {
  double p50;
  double p99;
  double maximum;
};

[[nodiscard]] Summary summarize(std::vector<double> samples) {
  std::sort(samples.begin(), samples.end());
  const auto percentile = [&samples](double value) {
    const std::size_t index = static_cast<std::size_t>(
        value * static_cast<double>(samples.size() - 1));
    return samples[index];
  };
  return {percentile(0.50), percentile(0.99), samples.back()};
}

template <class Table>
[[nodiscard]] std::vector<double> measure_continuous(
    std::size_t operation_count) {
  Table table;
  std::vector<double> samples;
  samples.reserve(operation_count);
  for (std::size_t key = 0; key < operation_count; ++key) {
    const auto start = Clock::now();
    table.insert_or_assign(key, key);
    const auto finish = Clock::now();
    samples.push_back(static_cast<double>(
        std::chrono::duration_cast<Nanoseconds>(finish - start)
            .count()));
  }
  if (table.size() != operation_count) {
    throw std::runtime_error("benchmark table lost entries");
  }
  return samples;
}

template <class Table>
[[nodiscard]] std::vector<double> measure_growth_triggers(
    const std::vector<std::size_t>& bucket_counts,
    std::size_t repetitions) {
  std::vector<double> samples;
  samples.reserve(bucket_counts.size() * repetitions);
  for (const std::size_t bucket_count : bucket_counts) {
    const std::size_t threshold = bucket_count * 3 / 4;
    for (std::size_t repeat = 0; repeat < repetitions; ++repeat) {
      Table table(bucket_count);
      for (std::size_t key = 0; key < threshold; ++key) {
        table.insert_or_assign(key, key);
      }
      const auto start = Clock::now();
      table.insert_or_assign(threshold, threshold);
      const auto finish = Clock::now();
      samples.push_back(static_cast<double>(
          std::chrono::duration_cast<Nanoseconds>(finish - start)
              .count()));
    }
  }
  return samples;
}

void print_summary(std::string_view workload,
                   std::string_view strategy,
                   const Summary& summary) {
  std::cout << std::left << std::setw(20) << workload
            << std::setw(14) << strategy << std::right
            << std::setw(12) << summary.p50 << std::setw(12)
            << summary.p99 << std::setw(12) << summary.maximum
            << '\n';
}

}  // namespace

int main() {
  using Incremental =
      mini_redis::HashTable<std::size_t, std::size_t>;
  using Synchronous =
      mini_redis::bench::SynchronousHashTable<std::size_t,
                                              std::size_t>;

  constexpr std::size_t kOperations = 300000;
  constexpr std::size_t kRepetitions = 12;
  const std::vector<std::size_t> bucket_counts{
      1024, 2048, 4096, 8192, 16384, 32768, 65536};

  const Summary synchronous_continuous =
      summarize(measure_continuous<Synchronous>(kOperations));
  const Summary incremental_continuous =
      summarize(measure_continuous<Incremental>(kOperations));
  const Summary synchronous_triggers = summarize(
      measure_growth_triggers<Synchronous>(bucket_counts,
                                           kRepetitions));
  const Summary incremental_triggers = summarize(
      measure_growth_triggers<Incremental>(bucket_counts,
                                           kRepetitions));

  std::cout << std::fixed << std::setprecision(0);
  std::cout << "latency_ns\n";
  std::cout << std::left << std::setw(20) << "workload"
            << std::setw(14) << "strategy" << std::right
            << std::setw(12) << "p50" << std::setw(12) << "p99"
            << std::setw(12) << "max" << '\n';
  print_summary("continuous_insert", "synchronous",
                synchronous_continuous);
  print_summary("continuous_insert", "incremental",
                incremental_continuous);
  print_summary("growth_trigger", "synchronous",
                synchronous_triggers);
  print_summary("growth_trigger", "incremental",
                incremental_triggers);
}

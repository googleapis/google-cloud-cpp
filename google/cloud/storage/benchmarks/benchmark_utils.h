// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_BENCHMARK_UTILS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_BENCHMARK_UTILS_H_

#include "google/cloud/internal/random.h"
#include "google/cloud/optional.h"
#include <chrono>
#include <cmath>
#include <functional>
#include <string>
#if GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
#include <sys/resource.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE

namespace google {
namespace cloud {
namespace storage_benchmarks {
/**
 * Create a random bucket name.
 *
 * Most benchmarks need to create a bucket to storage their data. Using a random
 * bucket name makes it possible to run different instances of the benchmark
 * without interacting with previous or concurrent instances.
 */
std::string MakeRandomBucketName(google::cloud::internal::DefaultPRNG& gen,
                                 std::string const& prefix);

/// Create a random object name.
std::string MakeRandomObjectName(google::cloud::internal::DefaultPRNG& gen);

/// Create a random local filename.
std::string MakeRandomFileName(google::cloud::internal::DefaultPRNG& gen);

/// Create a random chunk of data of a prescribed size.
std::string MakeRandomData(google::cloud::internal::DefaultPRNG& gen,
                           std::size_t desired_size);

constexpr std::int64_t kKiB = 1024;
constexpr std::int64_t kMiB = 1024 * kKiB;
constexpr std::int64_t kGiB = 1024 * kMiB;
constexpr std::int64_t kTiB = 1024 * kGiB;

constexpr std::int64_t kKB = 1000;
constexpr std::int64_t kMB = 1000 * kKB;
constexpr std::int64_t kGB = 1000 * kMB;
constexpr std::int64_t kTB = 1000 * kGB;

/// Parse a string as a byte size, with support for unit suffixes.
std::int64_t ParseSize(std::string const& val);

/// Parse a string as a duration with support for hours (h), minutes (m), or
/// second (s) suffixes.
std::chrono::seconds ParseDuration(std::string const& val);

/// Parse a string as a boolean, with a default value if the string is empty.
google::cloud::optional<bool> ParseBoolean(std::string const& val);

/// Defines a command-line option.
struct OptionDescriptor {
  using OptionParser = std::function<void(std::string const&)>;

  std::string option;
  std::string help;
  OptionParser parser;
};

/// Build the `Usage` string from a list of command-line option descriptions.
std::string BuildUsage(std::vector<OptionDescriptor> const& desc,
                       std::string const& command_path);

/**
 * Parse @p argv using the descriptions in @p desc, return unparsed arguments.
 */
std::vector<std::string> OptionsParse(std::vector<OptionDescriptor> const& desc,
                                      std::vector<std::string> argv);

class SimpleTimer {
 public:
  SimpleTimer() = default;

  /// Start the timer, call before the code being measured.
  void Start();

  /// Stop the timer, call after the code being measured.
  void Stop();

  //@{
  /**
   * @name Measurement results.
   *
   * @note The values are only valid after calling Start() and Stop().
   */
  std::chrono::microseconds elapsed_time() const { return elapsed_time_; }
  std::chrono::microseconds cpu_time() const { return cpu_time_; }
  std::string const& annotations() const { return annotations_; }
  //@}

  static bool SupportPerThreadUsage();

 private:
  std::chrono::steady_clock::time_point start_;
  std::chrono::microseconds elapsed_time_;
  std::chrono::microseconds cpu_time_;
#if GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
  struct rusage start_usage_;
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
  std::string annotations_;
};

/**
 * A distribution biased towards small values.
 *
 * The PDF for this distribution is 1/(x+1) normalized to be a proper
 * distribution.
 *
 * @par Rationale
 * Imagine you want to discover an optimal buffer size for an activity. You'll
 * likely run a large number of tests with different buffer sizes. If your
 * tested range is between 128K and 64M a uniform distribution will be wasteful.
 * It's unlikely that you'll need to decide whether 48M or 48.2M is better, but
 * quite likely that you'll need to decide between 128K and 256K. This
 * distribution will produce twice as many samples between 0 and 1 than between
 * 1 and 2 and so on.
 *
 * @par The formulas
 * PDF:
 * 1 / (x+1) / (ln(max+1) - ln(min+1))
 * CDF:
 * (ln(x+1) - ln(m+1)) / (ln(max+1) - ln(min+1))
 * Inverse CDF:
 * exp(x(ln(max+1) - ln(min+1)) + ln(min+1)) - 1
 */
class SmallValuesBiasedDistribution {
 public:
  SmallValuesBiasedDistribution(size_t min, size_t max)
      : l_min_(std::log(min + 1)), l_max_(std::log(max + 1)) {}

  using result_type = size_t;

  template <class Generator>
  result_type operator()(Generator& g) {
    return InvCDF((static_cast<double>(g()) - g.min()) / (g.max() - g.min()));
  }

  double PDF(result_type x) const;
  double CDF(result_type x) const;
  result_type InvCDF(double x) const;

 private:
  // Natural logarithms of (min_ + 1) and (max_ + 1).
  double l_min_;
  double l_max_;
};

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_BENCHMARK_UTILS_H_

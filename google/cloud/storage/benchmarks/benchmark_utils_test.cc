// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {

TEST(SmallValuesBiasedDistribution, PdfRanges) {
  SmallValuesBiasedDistribution d(100, 1000);
  EXPECT_NEAR(0, d.CDF(100), 0.00001);
  EXPECT_NEAR(1, d.CDF(1000), 0.00001);
  EXPECT_EQ(100, d.InvCDF(0));
  EXPECT_EQ(1000, d.InvCDF(1));
}

TEST(SmallValuesBiasedDistribution, PdfIntegration) {
  // Approximate expected value by using PDF to verify if it's close to the
  // expectation.
  //
  // The the expected value on range [m, M] is
  // (M - ln(M+1) + m - ln(m+1)) / (ln(M+1)-ln(m+1))

  SmallValuesBiasedDistribution::result_type m = 100;
  SmallValuesBiasedDistribution::result_type M = 100000;
  SmallValuesBiasedDistribution d(m, M);

  double ex = 0;

  double prev = m;
  for (size_t i = m + 1; i <= M; prev = i++) {
    double const prev_val = d.PDF(prev);
    double const val = d.PDF(i);
    double const avg = (prev * prev_val + i * val) / 2;
    ex += avg;
  }
  // Less than 1% difference.
  EXPECT_NEAR((M - std::log(M + 1) + m - std::log(m + 1)) /
                  (std::log(M + 1) - std::log(m + 1)),
              ex, ex / 100);
}

TEST(SmallValuesBiasedDistribution, RandomAverageConvergesToEx) {
  // Test if average over picking at random converges to expected value.

  SmallValuesBiasedDistribution::result_type m = 100;
  SmallValuesBiasedDistribution::result_type M = 100000;
  SmallValuesBiasedDistribution d(m, M);
  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();
  double sum = 0;
  int const num_samples = 1000000;
  for (int i = 0; i < num_samples; ++i) {
    sum += d(generator);
  }
  // Less than 1% difference.
  EXPECT_NEAR((M - std::log(M + 1) + m - std::log(m + 1)) /
                  (std::log(M + 1) - std::log(m + 1)),
              sum / num_samples, sum / 100);
}

}  // namespace
}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google

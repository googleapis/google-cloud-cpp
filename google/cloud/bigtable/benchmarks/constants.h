// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_CONSTANTS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_CONSTANTS_H_

namespace google {
namespace cloud {
namespace bigtable {
/// Supporting classes and functions to implement benchmarks.
namespace benchmarks {

//@{
/**
 * @name Test constants.
 *
 * Most of these were requirements in the original bugs (#189, #196).
 */
/// The size of the table.
constexpr long kDefaultTableSize = 10000000;

/// The name of the column family used in the benchmark.
constexpr char kColumnFamily[] = "cf";

/// The number of fields (aka columns, aka column qualifiers) in each row.
constexpr int kNumFields = 10;

/// The size of each value.
constexpr int kFieldSize = 100;

/// The size of each BulkApply request.
constexpr long kBulkSize = 1000;

/// The number of threads running the latency test.
constexpr int kDefaultThreads = 8;

/// How long does the test last by default.
constexpr int kDefaultTestDuration = 30;

/// How many shards are used to populate the table.
constexpr int kPopulateShardCount = 10;

/// How many times each PopulateTable shard reports progress.
constexpr int kPopulateShardProgressMarks = 4;

/// How many random bytes in the table id.
constexpr int kTableIdRandomLetters = 8;
//@}

}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_CONSTANTS_H_

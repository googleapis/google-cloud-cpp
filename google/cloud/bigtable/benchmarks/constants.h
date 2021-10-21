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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_CONSTANTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_CONSTANTS_H

#include <cinttypes>

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
std::int64_t constexpr kDefaultTableSize = 10000000L;

/// The name of the column family used in the benchmark.
constexpr char kColumnFamily[] = "cf";

/// The number of fields (aka columns, aka column qualifiers) in each row.
int constexpr kNumFields = 10;

/// The size of each value.
int constexpr kFieldSize = 100;

/// The size of each BulkApply request.
std::int64_t constexpr kBulkSize = 1000;

/// The number of threads running the latency test.
int constexpr kDefaultThreads = 8;

/// How long does the test last by default.
int constexpr kDefaultTestDuration = 30;

/// How many shards are used to populate the table.
int constexpr kPopulateShardCount = 10;

/// How many times each PopulateTable shard reports progress.
int constexpr kPopulateShardProgressMarks = 4;

/// How many random bytes in the table id.
int constexpr kTableIdRandomLetters = 8;
//@}

}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_CONSTANTS_H

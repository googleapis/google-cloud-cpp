// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/benchmarks/benchmark.h"
#include "google/cloud/internal/build_info.h"
#include <gmock/gmock.h>

using namespace google::cloud::bigtable::benchmarks;
using testing::HasSubstr;

namespace {
char arg0[] = "program";
char arg1[] = "foo";
char arg2[] = "bar";
char arg3[] = "4";
char arg4[] = "300";
char arg5[] = "10000";
char arg6[] = "True";
}  // anonymous namespace

TEST(BenchmarkTest, Create) {
  char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6};
  int argc = sizeof(argv) / sizeof(argv[0]);
  BenchmarkSetup setup("create", argc, argv);

  {
    Benchmark bm(setup);
    EXPECT_EQ(0, bm.create_table_count());
    std::string table_id = bm.CreateTable();
    EXPECT_EQ(1, bm.create_table_count());
    EXPECT_EQ(std::string("create-"), table_id.substr(0, 7));

    EXPECT_EQ(0, bm.delete_table_count());
    bm.DeleteTable();
    EXPECT_EQ(1, bm.delete_table_count());
  }
  SUCCEED() << "Benchmark object successfully destroyed";
}

TEST(BenchmarkTest, Populate) {
  char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6};
  int argc = sizeof(argv) / sizeof(argv[0]);
  BenchmarkSetup setup("populate", argc, argv);

  Benchmark bm(setup);
  bm.CreateTable();
  EXPECT_EQ(0, bm.mutate_rows_count());
  bm.PopulateTable();
  // The magic 10000 comes from arg5 and we accept 5% error.
  EXPECT_GE(int(10000 * 1.05 / kBulkSize), bm.mutate_rows_count());
  EXPECT_LE(int(10000 * 0.95 / kBulkSize), bm.mutate_rows_count());
  bm.DeleteTable();
}

TEST(BenchmarkTest, MakeRandomKey) {
  char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6};
  int argc = sizeof(argv) / sizeof(argv[0]);
  BenchmarkSetup setup("key", argc, argv);

  Benchmark bm(setup);
  auto gen = google::cloud::internal::MakeDefaultPRNG();

  // First make sure that the keys are not always the same.
  auto make_some_keys = [&bm, &gen]() {
    std::vector<std::string> keys(100);
    std::generate(keys.begin(), keys.end(),
                  [&bm, &gen]() { return bm.MakeRandomKey(gen); });
    return keys;
  };
  auto round0 = make_some_keys();
  auto round1 = make_some_keys();
  EXPECT_NE(round0, round1);

  // Also make sure the keys have the right format.
  for (auto const& k : round0) {
    EXPECT_EQ("user", k.substr(0, 4));
    std::string suffix = k.substr(5);
    EXPECT_FALSE(suffix.empty());
    EXPECT_EQ(std::string::npos, suffix.find_first_not_of("0123456789"));
  }
}

TEST(BenchmarkTest, PrintThroughputResult) {
  char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6};
  int argc = sizeof(argv) / sizeof(argv[0]);
  BenchmarkSetup setup("throughput", argc, argv);

  Benchmark bm(setup);
  BenchmarkResult result{};
  result.elapsed = std::chrono::milliseconds(10000);
  result.row_count = 1230;
  result.operations.resize(3450);

  std::ostringstream os;
  bm.PrintThroughputResult(os, "foo", "bar", result);
  std::string output = os.str();

  // We do not want a change detector test, so the following assertions are
  // fairly minimal.

  // The output includes "XX ops/s" where XX is the operations count.
  EXPECT_THAT(output, HasSubstr("345 ops/s"));

  // The output includes "YY rows/s" where YY is the row count.
  EXPECT_THAT(output, HasSubstr("123 rows/s"));
}

TEST(BenchmarkTest, PrintLatencyResult) {
  char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6};
  int argc = sizeof(argv) / sizeof(argv[0]);
  BenchmarkSetup setup("latency", argc, argv);

  Benchmark bm(setup);
  BenchmarkResult result{};
  result.elapsed = std::chrono::milliseconds(1000);
  result.row_count = 100;
  result.operations.resize(100);
  int count = 0;
  std::generate(result.operations.begin(), result.operations.end(), [&count]() {
    return OperationResult{true, std::chrono::microseconds(++count * 100)};
  });

  std::ostringstream os;
  bm.PrintLatencyResult(os, "foo", "bar", result);
  std::string output = os.str();

  // We do not want a change detector test, so the following assertions are
  // fairly minimal.

  // The output includes "XX ops/s" where XX is the operations count.
  EXPECT_THAT(output, HasSubstr("100 ops/s"));

  // And the percentiles are easy to estimate for the generated data. Note that
  // this test depends on the duration formatting as specified by the absl::time
  // library.
  EXPECT_THAT(output, HasSubstr("p0=100.000us"));
  EXPECT_THAT(output, HasSubstr("p95=9.500ms"));
  EXPECT_THAT(output, HasSubstr("p100=10.000ms"));
}

TEST(BenchmarkTest, PrintCsv) {
  char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6};
  int argc = sizeof(argv) / sizeof(argv[0]);
  BenchmarkSetup setup("latency", argc, argv);

  Benchmark bm(setup);
  BenchmarkResult result{};
  result.elapsed = std::chrono::milliseconds(1000);
  result.row_count = 123;
  result.operations.resize(100);
  int count = 0;
  std::generate(result.operations.begin(), result.operations.end(), [&count]() {
    return OperationResult{true, std::chrono::microseconds(++count * 100)};
  });

  std::string header = bm.ResultsCsvHeader();
  auto const field_count = std::count(header.begin(), header.end(), ',');

  std::ostringstream os;
  bm.PrintResultCsv(os, "foo", "bar", "latency", result);
  std::string output = os.str();

  auto const actual_count = std::count(output.begin(), output.end(), ',');
  EXPECT_EQ(field_count, actual_count);

  // We do not want a change detector test, so the following assertions are
  // fairly minimal.

  // The output includes the version and compiler info.
  EXPECT_THAT(output, HasSubstr(google::cloud::bigtable::version_string()));
  EXPECT_THAT(output, HasSubstr(google::cloud::internal::compiler()));
  EXPECT_THAT(output, HasSubstr(google::cloud::internal::compiler_flags()));

  // The output includes the latency results.
  EXPECT_THAT(output, HasSubstr(",100,"));    // p0
  EXPECT_THAT(output, HasSubstr(",9500,"));   // p95
  EXPECT_THAT(output, HasSubstr(",10000,"));  // p100

  // The output includes the throughput.
  EXPECT_THAT(output, HasSubstr(",123,"));
}

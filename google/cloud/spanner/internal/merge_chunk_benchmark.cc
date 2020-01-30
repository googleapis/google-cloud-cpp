// Copyright 2020 Google LLC
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

#include "google/cloud/spanner/internal/merge_chunk.h"
#include "google/cloud/spanner/value.h"
#include <benchmark/benchmark.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

//
// MakeProtoValue() is an overloaded helper function for creating
// google::protobuf::Value protos from convenient user-supplied arguments.
//

google::protobuf::Value MakeProtoValue(Value v) {
  google::protobuf::Value value;
  std::tie(std::ignore, value) = internal::ToProto(std::move(v));
  return value;
}

google::protobuf::Value MakeProtoValue(std::string s) {
  google::protobuf::Value value;
  value.set_string_value(std::move(s));
  return value;
}

google::protobuf::Value MakeProtoValue(double d) {
  google::protobuf::Value value;
  value.set_number_value(d);
  return value;
}

template <typename T>
google::protobuf::Value MakeProtoValue(std::vector<T> v) {
  google::protobuf::Value value;
  for (auto& e : v) {
    *value.mutable_list_value()->add_values() = MakeProtoValue(std::move(e));
  }
  return value;
}

// Run on (6 X 2300 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x3)
//   L1 Instruction 32K (x3)
//   L2 Unified 256K (x3)
//   L3 Unified 46080K (x1)
// Load Average: 0.14, 1.27, 1.77
// --------------------------------------------------------------------------
// Benchmark                                Time             CPU   Iterations
// --------------------------------------------------------------------------
// BM_MergeChunkStrings                  95.0 ns         94.4 ns      7408343
// BM_MergeChunkListOfInts                317 ns          315 ns      2208054
// BM_MergeChunkListOfStrings             481 ns          480 ns      1000000
// BM_MergeChunkListsOfListOfString       817 ns          809 ns       837894

void BM_MergeChunkStrings(benchmark::State& state) {
  auto const a = MakeProtoValue("foo");
  auto const b = MakeProtoValue("bar");
  for (auto _ : state) {
    auto value = a;
    auto chunk = b;
    benchmark::DoNotOptimize(MergeChunk(value, std::move(chunk)));
  }
}
BENCHMARK(BM_MergeChunkStrings);

void BM_MergeChunkListOfInts(benchmark::State& state) {
  auto const a = MakeProtoValue(std::vector<double>{2, 3});
  auto const b = MakeProtoValue(std::vector<double>{4});
  for (auto _ : state) {
    auto value = a;
    auto chunk = b;
    benchmark::DoNotOptimize(MergeChunk(value, std::move(chunk)));
  }
}
BENCHMARK(BM_MergeChunkListOfInts);

void BM_MergeChunkListOfStrings(benchmark::State& state) {
  auto const a = MakeProtoValue(std::vector<std::string>{"a", "b"});
  auto const b = MakeProtoValue(std::vector<std::string>{"c", "d"});
  for (auto _ : state) {
    auto value = a;
    auto chunk = b;
    benchmark::DoNotOptimize(MergeChunk(value, std::move(chunk)));
  }
}
BENCHMARK(BM_MergeChunkListOfStrings);

void BM_MergeChunkListsOfListOfString(benchmark::State& state) {
  auto const a = MakeProtoValue(std::vector<Value>{
      Value("a"), Value(std::vector<std::string>{"b", "c"})});
  auto const b = MakeProtoValue(
      std::vector<Value>{Value(std::vector<std::string>{"d"}), Value("e")});
  for (auto _ : state) {
    auto value = a;
    auto chunk = b;
    benchmark::DoNotOptimize(MergeChunk(value, std::move(chunk)));
  }
}
BENCHMARK(BM_MergeChunkListsOfListOfString);

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

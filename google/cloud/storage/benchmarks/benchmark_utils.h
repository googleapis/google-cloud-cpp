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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_BENCHMARK_UTILS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_BENCHMARK_UTILS_H

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/random_names.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/command_line_parsing.h"
#include "google/cloud/testing_util/timer.h"
#include <string>

namespace google {
namespace cloud {
namespace storage_benchmarks {

using ::google::cloud::storage::testing::MakeRandomData;
using ::google::cloud::storage::testing::MakeRandomFileName;
using ::google::cloud::storage::testing::MakeRandomObjectName;
using ::google::cloud::testing_util::kGB;
using ::google::cloud::testing_util::kGiB;
using ::google::cloud::testing_util::kKB;
using ::google::cloud::testing_util::kKiB;
using ::google::cloud::testing_util::kMB;
using ::google::cloud::testing_util::kMiB;
using ::google::cloud::testing_util::kTB;
using ::google::cloud::testing_util::kTiB;
using ::google::cloud::testing_util::ParseBoolean;
using ::google::cloud::testing_util::ParseBufferSize;
using ::google::cloud::testing_util::ParseDuration;
using ::google::cloud::testing_util::ParseSize;
using ::google::cloud::testing_util::Timer;

void DeleteAllObjects(google::cloud::storage::Client client,
                      std::string const& bucket_name, int thread_count);
void DeleteAllObjects(google::cloud::storage::Client client,
                      std::string const& bucket_name,
                      google::cloud::storage::Prefix prefix, int thread_count);

// Technically gRPC is not a different API, just the JSON API over a different
// protocol, but it is easier to represent it as such in the benchmark.
enum class ApiName {
  kApiJson,
  kApiXml,
  kApiGrpc,
  kApiRawJson,
  kApiRawXml,
  kApiRawGrpc,
};
char const* ToString(ApiName api);

StatusOr<ApiName> ParseApiName(std::string const& val);

std::string RandomBucketPrefix();

std::string MakeRandomBucketName(google::cloud::internal::DefaultPRNG& gen);

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_BENCHMARK_UTILS_H

// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// This test depends on Linux-specific features.
#if GTEST_OS_LINUX
using ::google::cloud::internal::GetEnv;

class ObjectReadLargeIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

std::size_t CurrentRss() {
  auto is = std::ifstream("/proc/self/statm");
  std::vector<std::string> fields = absl::StrSplit(
      std::string{std::istreambuf_iterator<char>{is.rdbuf()}, {}}, ' ');
  // The fields are documented in proc(5).
  return static_cast<std::size_t>(std::stoull(fields[1])) * 4096;
}

std::string DebugRss() {
  std::ostringstream os;
  for (auto const* path : {"/proc/self/status", "/proc/self/maps"}) {
    os << "\n" << path << "\n";
    auto is = std::ifstream(path);
    os << std::string{std::istreambuf_iterator<char>{is.rdbuf()}, {}};
  }
  return std::move(os).str();
}

TEST_F(ObjectReadLargeIntegrationTest, LimitedMemoryGrowth) {
  auto client = MakeIntegrationTestClient();
  auto bucket_name = GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME");
  ASSERT_TRUE(bucket_name.has_value());

  // This environment variable is not defined in the CI builds. It can be used
  // to override the object in manual tests.
  auto object_name = GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_OBJECT_NAME_HUGE");
  if (!object_name) {
    object_name = MakeRandomObjectName();
    auto data = MakeRandomData(10 * 1024 * 1024);
    auto meta =
        client.InsertObject(*bucket_name, *object_name, std::move(data));
    ASSERT_STATUS_OK(meta);
    ScheduleForDelete(*meta);
  }

  auto constexpr kBufferSize = 128 * 1024;
  auto constexpr kRssTolerance = 32 * 1024 * 1024;
  std::vector<char> buffer(kBufferSize);
  auto reader = client.ReadObject(*bucket_name, *object_name);
  auto const initial_rss = CurrentRss();
  std::cout << "Initial RSS = " << initial_rss << DebugRss() << std::endl;
  auto tolerance = initial_rss + kRssTolerance;
  std::int64_t offset = 0;
  while (reader) {
    reader.read(buffer.data(), buffer.size());
    offset += reader.gcount();
    auto const current_rss = CurrentRss();
    EXPECT_LE(current_rss, tolerance) << "offset=" << offset << DebugRss();
    if (current_rss >= tolerance) tolerance = current_rss + kRssTolerance;
  }
  EXPECT_STATUS_OK(reader.status());
}
#endif  // GTEST_OS_LINUX

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

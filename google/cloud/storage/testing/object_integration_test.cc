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

#include "google/cloud/storage/testing/object_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/status.h"
#include <gmock/gmock.h>
#include <cstddef>
#include <cstdlib>
#ifdef __unix__
#include <dirent.h>
#endif  // __unix__
#include <string>

namespace google {
namespace cloud {
namespace storage {
namespace testing {
namespace {

StatusOr<std::size_t> GetNumEntries(std::string const& path) {
#ifdef __unix__
  DIR* dir = opendir(path.c_str());
  if (dir == nullptr) {
    return Status(StatusCode::kInternal, "Failed to open directory \"" + path +
                                             "\": " + strerror(errno));
  }
  std::size_t count = 0;
  while (readdir(dir)) {
    ++count;
  }
  closedir(dir);
  return count;
#else   // __unix__
  return Status(StatusCode::kUnimplemented,
                "Can't check #entries in " + path +
                    ", because only UNIX systems are supported");
#endif  // __unix__
}

}  // anonymous namespace

StatusOr<std::size_t> ObjectIntegrationTest::GetNumOpenFiles() {
  auto res = GetNumEntries("/proc/self/fd");
  if (!res) return res;
  if (*res < 3) {
    return Status(StatusCode::kInternal,
                  "Expected at least three entries in /proc/self/fd: ., .., "
                  "and /proc/self/fd itself, found " +
                      std::to_string(*res));
  }
  return *res - 3;
}

void ObjectIntegrationTest::SetUp() {
  project_id_ =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_FALSE(project_id_.empty());
  bucket_name_ = google::cloud::internal::GetEnv(
                     "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                     .value_or("");
  ASSERT_FALSE(bucket_name_.empty());
}

std::string ObjectIntegrationTest::MakeEntityName() const {
  // We always use the viewers for the project because it is known to exist.
  return "project-viewers-" + project_id_;
}

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

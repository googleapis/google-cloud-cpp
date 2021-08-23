// Copyright 2021 Google LLC
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

#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/internal/format_time_point.h"

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

// Unless otherwise noted, the maximum ID lengths discovered by trial and error.
auto constexpr kMaxTableIdLength = 50;
char const kRandomTableIdRE[] = R"re(^tbl-\d{4}-\d{2}-\d{2}-.*$)re";

// Per google/bigtable/admin/v2/bigtable_table_admin.proto, backup names must
// be between 1 and 50 characters such, [_a-zA-Z0-9][-_.a-zA-Z0-9]*.
auto constexpr kMaxBackupIdLength = 50;
char const kRandomBackupIdRE[] = R"re(^bck-\d{4}-\d{2}-\d{2}-.*$)re";

auto constexpr kMaxClusterIdLength = 30;
char const kRandomClusterIdRE[] = R"re(^cl-\d{4}-\d{2}-\d{2}-.*$)re";

// Cloud Bigtable instance ids must have at least 6 characters, and can have
// up to 33 characters. But many of the examples append `-c1` or `-c2` to
// create cluster ids based on the instance id. So we make the generated ids
// even shorter.
auto constexpr kMaxInstanceIdLength = kMaxClusterIdLength - 3;
char const kRandomInstanceIdRE[] = R"re(^it-\d{4}-\d{2}-\d{2}-.*$)re";

std::string RandomTableId(google::cloud::internal::DefaultPRNG& generator,
                          std::chrono::system_clock::time_point tp) {
  auto const prefix = RandomTableId(tp);
  auto size = static_cast<int>(kMaxTableIdLength - 1 - prefix.size());
  return prefix + google::cloud::internal::Sample(
                      generator, size, "abcdefghijlkmnopqrstuvwxyz0123456789");
}

std::string RandomTableId(std::chrono::system_clock::time_point tp) {
  std::string date = google::cloud::internal::FormatUtcDate(tp);
  return "tbl-" + date + "-";
}

std::string RandomTableIdRegex() { return kRandomTableIdRE; }

std::string RandomBackupId(google::cloud::internal::DefaultPRNG& generator,
                           std::chrono::system_clock::time_point tp) {
  auto const prefix = RandomBackupId(tp);
  auto size = static_cast<int>(kMaxBackupIdLength - 1 - prefix.size());
  return prefix + google::cloud::internal::Sample(
                      generator, size, "abcdefghijlkmnopqrstuvwxyz0123456789");
}

std::string RandomBackupId(std::chrono::system_clock::time_point tp) {
  std::string date = google::cloud::internal::FormatUtcDate(tp);
  return "bck-" + date + "-";
}

std::string RandomBackupIdRegex() { return kRandomBackupIdRE; }

std::string RandomClusterId(google::cloud::internal::DefaultPRNG& generator,
                            std::chrono::system_clock::time_point tp) {
  auto const prefix = RandomClusterId(tp);
  auto size = static_cast<int>(kMaxClusterIdLength - 1 - prefix.size());
  return prefix + google::cloud::internal::Sample(
                      generator, size, "abcdefghijlkmnopqrstuvwxyz0123456789");
}

std::string RandomClusterId(std::chrono::system_clock::time_point tp) {
  auto const date = google::cloud::internal::FormatUtcDate(tp);
  return "cl-" + date + "-";
}

std::string RandomClusterIdRegex() { return kRandomClusterIdRE; }

std::string RandomInstanceId(google::cloud::internal::DefaultPRNG& generator,
                             std::chrono::system_clock::time_point tp) {
  auto const prefix = RandomInstanceId(tp);
  auto size = static_cast<int>(kMaxInstanceIdLength - 1 - prefix.size());
  return prefix + google::cloud::internal::Sample(
                      generator, size, "abcdefghijlkmnopqrstuvwxyz0123456789");
}

std::string RandomInstanceId(std::chrono::system_clock::time_point tp) {
  auto const date = google::cloud::internal::FormatUtcDate(tp);
  return "it-" + date + "-";
}

std::string RandomInstanceIdRegex() { return kRandomInstanceIdRE; }

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

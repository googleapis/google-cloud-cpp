// Copyright 2026 Google LLC
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

#include "google/cloud/spanner/internal/spanner_request_id.h"
#include "google/cloud/internal/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include <atomic>
#include <mutex>
#include <random>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string ProcessRandomId() {
#ifndef _WIN32
  static std::mutex mu;
  static pid_t pid = 0;
  static std::string random_id;
  pid_t const current_pid = getpid();
  std::scoped_lock lock(mu);
  if (pid != current_pid) {
    auto generator = google::cloud::internal::MakeDefaultPRNG();
    std::uniform_int_distribution<std::uint64_t> dist;
    random_id = absl::StrFormat("%016x", dist(generator));
    pid = current_pid;
  }
  return random_id;
#else
  static std::string const random_id = [] {
    auto generator = google::cloud::internal::MakeDefaultPRNG();
    std::uniform_int_distribution<std::uint64_t> dist;
    return absl::StrFormat("%016x", dist(generator));
  }();
  return random_id;
#endif
}

std::uint64_t NextClientId() {
  static std::atomic<std::uint64_t> counter{0};
  return ++counter;
}

std::string FormatSpannerRequestStaticPrefix(std::uint32_t version,
                                             std::string_view process_random_id,
                                             std::uint64_t client_id) {
  return absl::StrCat(version, ".", process_random_id, ".", client_id, ".");
}

std::string FormatSpannerRequestId(std::string_view static_prefix,
                                   std::uint32_t channel_id,
                                   std::uint64_t request_index,
                                   std::uint32_t attempt_index) {
  return absl::StrCat(static_prefix, channel_id, ".", request_index, ".",
                      attempt_index);
}

std::string FormatSpannerRequestId(std::uint32_t version,
                                   std::string_view process_random_id,
                                   std::uint64_t client_id,
                                   std::uint32_t channel_id,
                                   std::uint64_t request_index,
                                   std::uint32_t attempt_index) {
  return absl::StrCat(version, ".", process_random_id, ".", client_id, ".",
                      channel_id, ".", request_index, ".", attempt_index);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

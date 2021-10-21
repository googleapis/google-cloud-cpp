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

#include "google/cloud/connection_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_split.h"
#include <sstream>
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
std::set<std::string> DefaultTracingComponents() {
  auto tracing =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_ENABLE_TRACING");
  if (!tracing.has_value()) return {};
  return absl::StrSplit(*tracing, ',');
}

TracingOptions DefaultTracingOptions() {
  auto tracing_options =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_TRACING_OPTIONS");
  if (!tracing_options) return {};
  return TracingOptions{}.SetOptions(*tracing_options);
}

std::unique_ptr<BackgroundThreads> DefaultBackgroundThreads(
    std::size_t thread_pool_size) {
  return absl::make_unique<AutomaticallyCreatedBackgroundThreads>(
      thread_pool_size);
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

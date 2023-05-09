// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_COMMON_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_COMMON_OPTIONS_H

#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <memory>
#include <thread>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

auto constexpr kBackoffScaling = 2.0;
std::size_t constexpr kConnectionPoolSize = 4;
std::size_t constexpr kConnectionPoolSizeMax = 64;

inline std::size_t DefaultConnectionPoolSize() {
  // For better resource utilization and greater throughput, it is recommended
  // to calculate the default pool size based on cores(CPU) available. However,
  // as per C++11 documentation `std::thread::hardware_concurrency()` cannot be
  // fully relied upon. It is only a hint and the value can be 0 if it is not
  // well defined or not computable. Apart from CPU count, multiple channels
  // can be opened for each CPU to increase throughput. The pool size is also
  // capped so that servers with many cores do not create too many channels.
  int cpu_count = std::thread::hardware_concurrency();
  if (cpu_count == 0) return kConnectionPoolSize;
  return (std::min)(kConnectionPoolSizeMax, cpu_count * kConnectionPoolSize);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_COMMON_OPTIONS_H

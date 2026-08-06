// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OPEN_OBJECT_METRICS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OPEN_OBJECT_METRICS_H

#include "google/cloud/version.h"
#include <chrono>
#include <string>

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/trace/span.h>
#endif

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class OpenObjectMetrics {
 public:
  void RecordCall();
  void RecordStart();
  void RecordWrite();

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  void RecordRead(
      std::string const& bucket,
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> const& span);
#else
  void RecordRead(std::string const& bucket);
#endif

 private:
#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
  void RecordMetrics(std::string const& bucket,
                     std::chrono::steady_clock::time_point t3);

  std::chrono::steady_clock::time_point t0_;
  std::chrono::steady_clock::time_point t1_;
  std::chrono::steady_clock::time_point t2_;
#endif
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OPEN_OBJECT_METRICS_H

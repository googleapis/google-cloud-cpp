// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_TRACING_OBJECT_READ_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_TRACING_OBJECT_READ_SOURCE_H

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/storage/internal/object_read_source.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/opentelemetry.h"
#include <memory>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 *
 */
class TracingObjectReadSource : public storage::internal::ObjectReadSource {
 public:
  TracingObjectReadSource(
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
      std::unique_ptr<storage::internal::ObjectReadSource> child);
  ~TracingObjectReadSource() override;

  bool IsOpen() const override;
  StatusOr<storage::internal::HttpResponse> Close() override;
  StatusOr<storage::internal::ReadSourceResult> Read(char* buf,
                                                     std::size_t n) override;

 private:
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
  std::unique_ptr<storage::internal::ObjectReadSource> child_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_TRACING_OBJECT_READ_SOURCE_H

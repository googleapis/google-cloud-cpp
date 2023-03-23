// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TRACED_STREAM_RANGE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TRACED_STREAM_RANGE_H

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/stream_range.h"
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/span.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

template <typename T>
class TracedStreamRange {
 public:
  explicit TracedStreamRange(
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
      StreamRange<T> sr)
      : span_(std::move(span)), sr_(std::move(sr)), it_(sr_.begin()) {}

  ~TracedStreamRange() {
    // It is ok not to iterate the full range. We should still end our span.
    (void)End(Status());
  }

  absl::variant<Status, T> Advance() {
    if (it_ == sr_.end()) return End(Status());
    auto sor = *it_;
    ++it_;
    if (!sor) return End(std::move(sor).status());
    return *sor;
  }

 private:
  Status End(Status const& s) { return EndSpan(*span_, s); }

  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
  StreamRange<T> sr_;
  typename StreamRange<T>::iterator it_;
};

/**
 * Makes a traced `StreamRange<>`.
 *
 * The span that wraps the operation is complete when either the range is
 * iterated over, or the returned object goes out of scope.
 *
 * Note that when a `StreamRange<>` is constructed, it preloads the first value.
 * In order for any sub-operations to be tied to the parent `span`, it must be
 * made active (by creating a `Scope` object).
 */
template <typename T>
StreamRange<T> MakeTracedStreamRange(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
    StreamRange<T> sr) {
  // StreamRange is not copyable, but a shared ptr to one is.
  auto impl =
      std::make_shared<TracedStreamRange<T>>(std::move(span), std::move(sr));
  auto reader = [impl = std::move(impl)] { return impl->Advance(); };
  return internal::MakeStreamRange<T>(std::move(reader));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TRACED_STREAM_RANGE_H

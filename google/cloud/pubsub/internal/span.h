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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SPAN_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SPAN_H

#include "google/cloud/pubsub/version.h"
#include <memory>
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/trace/span.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Span class stores an open telemetery span which can only be accessed by code
 * compiled with GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY.
 */
class Span {
 public:
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  void SetSpan(
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span) {
    span_ = std::move(span);
  }

  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> GetSpan() {
    return span_;
  }
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

 private:
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SPAN_H

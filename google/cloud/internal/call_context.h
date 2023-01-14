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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CALL_CONTEXT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CALL_CONTEXT_H

#include "google/cloud/internal/opentelemetry_aliases.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <chrono>
#include <functional>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * The context in which a client call is made.
 *
 * This class contains the configuration options set by the user. If tracing is
 * enabled, it also contains the parent span that encompasses the client call.
 */
struct CallContext {
  Options options = CurrentOptions();
  Span span = CurrentSpan();
};

// For propagating context, typically across threads in async operations.
class ScopedCallContext {
 public:
  explicit ScopedCallContext(CallContext call_context)
      : options_(std::move(call_context.options)),
        span_(std::move(call_context.span)) {}

 private:
  using ScopedOptions = OptionsSpan;
  ScopedOptions options_;
  ScopedSpan span_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CALL_CONTEXT_H

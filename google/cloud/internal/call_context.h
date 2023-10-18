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

#include "google/cloud/internal/opentelemetry_context.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/tracer.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
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
  explicit CallContext(ImmutableOptions o) : options(std::move(o)) {}
  // TODO(#12359) - maybe this can be removed once the explicit options cleanup
  //     is done.
  CallContext() : CallContext(SaveCurrentOptions()) {}

  ImmutableOptions options;
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  OTelContext otel_context = CurrentOTelContext();
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
};

/**
 * For propagating context, typically across threads in async operations.
 *
 * This class holds the member(s) of `CallContext` in scoped classes.
 */
class ScopedCallContext {
 public:
  explicit ScopedCallContext(CallContext call_context)
      : options_span_(std::move(call_context.options))
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
        // clang-format off
        , scoped_otel_context_(std::move(call_context.otel_context))
  // clang-format on
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  {
  }

  // `ScopedCallContext` should not be copied/moved.
  ScopedCallContext(ScopedCallContext const&) = delete;
  ScopedCallContext(ScopedCallContext&&) = delete;
  ScopedCallContext& operator=(ScopedCallContext const&) = delete;
  ScopedCallContext& operator=(ScopedCallContext&&) = delete;

  // `ScopedCallContext` should only be used for block-scoped objects.
  static void* operator new(std::size_t) = delete;
  static void* operator new[](std::size_t) = delete;

 private:
  OptionsSpan options_span_;
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  ScopedOTelContext scoped_otel_context_;
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CALL_CONTEXT_H

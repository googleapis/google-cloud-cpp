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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPENTELEMETRY_CONTEXT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPENTELEMETRY_CONTEXT_H

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/version.h"
#include <opentelemetry/context/context.h>
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/span.h>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Represents the stack of active spans that have been created by *our* library.
 *
 * Typically OpenTelemetry handles this for us, but in the case of asynchronous
 * APIs, we need to keep track of this stuff manually.
 *
 * For GAPICs, the size never exceeds 2 (the connection span, and the stub
 * span). In handwritten libraries like Pub/Sub this number may be higher. It
 * probably won't ever be more than 5.
 *
 * For more details on the design of these functions, Googlers can see
 * go/cloud-cxx:otel-async-context-dd.
 */
using OTelContext = std::vector<opentelemetry::context::Context>;

OTelContext CurrentOTelContext();

/**
 * Attach the supplied context to the RuntimeContext, storing its Token.
 *
 * This is called when we jump from one thread to another, to restore the
 * initial threads context (i.e. its active spans).
 */
void AttachOTelContext(opentelemetry::context::Context const& context);
/**
 * Detach the supplied context from the RuntimeContext using its Token.
 *
 * This is called from any `.then()`s which might execute in a different thread.
 * For example, when we `EndSpan(..., future<T>)`.
 */
void DetachOTelContext(opentelemetry::context::Context const& context);

/**
 * A wrapper around an `opentelemetry::trace::Scope` object that maintains our
 * `OTelContext` stack.
 *
 * Upon construction, a `Scope` object is created. The current `Context` is
 * pushed to our `OTelContext` stack. Upon destruction, the `Scope` object is
 * destroyed and the `Context` is popped from our `OTelContext` stack.
 *
 * We need to maintain our own context stack for async operations where the
 * default OpenTelemetry storage is not sufficient.
 *
 * @code
 * {
 *   auto span = MakeSpan("span");
 *   OTelScope scope(span);
 *   // Perform work while `span` is active.
 * }
 * @endcode
 */
class OTelScope {
 public:
  explicit OTelScope(
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> const& span);

  ~OTelScope();

 private:
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
  opentelemetry::context::Context context_;
};

/**
 * If the supplied OTelContext is not currently active, this class attaches it
 * when constructed, and detaches it when destructed.
 */
class ScopedOTelContext {
 public:
  explicit ScopedOTelContext(OTelContext contexts)
      : contexts_(std::move(contexts)),
        noop_(contexts_.empty() ||
              contexts_.back() ==
                  opentelemetry::context::RuntimeContext::GetCurrent()) {
    if (noop_) return;
    for (auto const& c : contexts_) {
      AttachOTelContext(c);
    }
  }

  ~ScopedOTelContext() {
    if (noop_) return;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = contexts_.rbegin(); it != contexts_.rend(); ++it) {
      DetachOTelContext(*it);
    }
  }

 private:
  OTelContext contexts_;
  bool noop_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPENTELEMETRY_CONTEXT_H

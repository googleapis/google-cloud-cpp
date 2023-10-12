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
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Represents the stack of active spans that have been created by our library.
 *
 * Typically OpenTelemetry handles this for us, but in the case of asynchronous
 * APIs, we need to keep track of this stuff manually.
 */
using OTelContext = std::vector<opentelemetry::context::Context>;

OTelContext CurrentOTelContext();

/// Append the current context to our thread local stack.
void PushOTelContext();
/// Pop the current context from our thread local stack.
void PopOTelContext();

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

class ScopedOTelContext {
 public:
  explicit ScopedOTelContext(OTelContext contexts)
      : contexts_(std::move(contexts)) {
    for (auto const& c : contexts_) {
      AttachOTelContext(c);
    }
  }

  ~ScopedOTelContext() {
    for (auto const& c : contexts_) {
      DetachOTelContext(c);
    }
  }

 private:
  OTelContext contexts_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPENTELEMETRY_CONTEXT_H

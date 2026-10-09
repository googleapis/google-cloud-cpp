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

#include "google/cloud/internal/opentelemetry_context.h"
#include "google/cloud/internal/call_context.h"
#include <opentelemetry/nostd/unique_ptr.h>
#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using OTelContextWithTokens = std::vector<
    std::pair<opentelemetry::context::Context,
              opentelemetry::nostd::unique_ptr<opentelemetry::context::Token>>>;

/**
 * Holds a context stack to be used when tracing asynchronous APIs.
 *
 * Note that opentelemetry::context::Context is a pimpl class. Copying it is
 * cheap.
 */
OTelContextWithTokens& ThreadLocalOTelContext() {
  thread_local auto context = OTelContextWithTokens{};
  return context;
}

}  // namespace

OTelContext CurrentOTelContext() {
  auto const& v = ThreadLocalOTelContext();
  OTelContext contexts;
  std::transform(v.begin(), v.end(), std::back_inserter(contexts),
                 [](auto const& p) { return p.first; });
  return contexts;
}

void AttachOTelContext(opentelemetry::context::Context const& context) {
  auto& v = ThreadLocalOTelContext();
  auto token = opentelemetry::context::RuntimeContext::Attach(context);
  v.emplace_back(context, std::move(token));
}

void DetachOTelContext(opentelemetry::context::Context const& context) {
  auto& v = ThreadLocalOTelContext();
  if (!v.empty() && context == v.back().first) v.pop_back();
}

OTelScope::OTelScope(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> const& span)
    : span_(span) {
  auto& v = ThreadLocalOTelContext();
  context_ = opentelemetry::context::RuntimeContext::GetCurrent().SetValue(
      opentelemetry::trace::kSpanKey, span);
  // Manually attach the context. We do not use an opentelemetry::trace::Scope,
  // because we need to own the token.
  auto token = opentelemetry::context::RuntimeContext::Attach(context_);
  v.emplace_back(context_, std::move(token));
}

OTelScope::~OTelScope() { DetachOTelContext(context_); }

CallContext::CallContext(ImmutableOptions o)
    : options(std::move(o)), otel_context(CurrentOTelContext()) {}

CallContext::CallContext() : CallContext(SaveCurrentOptions()) {}

CallContext::~CallContext() = default;
CallContext::CallContext(CallContext const&) = default;
CallContext::CallContext(CallContext&&) noexcept = default;
CallContext& CallContext::operator=(CallContext const&) = default;
CallContext& CallContext::operator=(CallContext&&) noexcept = default;

ScopedCallContext::ScopedCallContext(CallContext call_context)
    : options_span_(std::move(call_context.options)),
      contexts_(std::move(call_context.otel_context)),
      noop_(contexts_.empty() ||
            contexts_.back() ==
                opentelemetry::context::RuntimeContext::GetCurrent()) {
  if (noop_) return;
  for (opentelemetry::context::Context const& c : contexts_) {
    AttachOTelContext(c);
  }
}

ScopedCallContext::~ScopedCallContext() {
  if (noop_) return;
  // NOLINTNEXTLINE(modernize-loop-convert)
  for (auto it = contexts_.rbegin(); it != contexts_.rend(); ++it) {
    opentelemetry::context::Context const& c = *it;
    DetachOTelContext(c);
  }
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

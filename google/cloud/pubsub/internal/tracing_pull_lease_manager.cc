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

#include "google/cloud/pubsub/internal/tracing_pull_lease_manager.h"
#include "google/cloud/pubsub/internal/pull_lease_manager.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/internal/async_retry_loop.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/status.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/pubsub/internal/tracing_helpers.h"
#include "opentelemetry/trace/semantic_conventions.h"
#include "opentelemetry/trace/span.h"
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

class TracingPullLeaseManager : public PullLeaseManager {
 public:
  explicit TracingPullLeaseManager(std::shared_ptr<PullLeaseManager> child)
      : child_(std::move(child)),
        consumer_span_context_(
            opentelemetry::trace::GetSpan(
                opentelemetry::context::RuntimeContext::GetCurrent())
                ->GetContext()) {}

  void StartLeaseLoop() override { child_->StartLeaseLoop(); };

  std::chrono::milliseconds LeaseRefreshPeriod() const override {
    return child_->LeaseRefreshPeriod();
  }

  future<Status> ExtendLease(std::shared_ptr<SubscriberStub> stub,
                             std::chrono::system_clock::time_point time_now,
                             std::chrono::seconds extension) override {
    namespace sc = opentelemetry::trace::SemanticConventions;
    opentelemetry::trace::StartSpanOptions options;
    options.kind = opentelemetry::trace::SpanKind::kClient;
    auto span = internal::MakeSpan(
        child_->subscription().subscription_id() + " modack",
        {{sc::kMessagingSystem, "gcp_pubsub"},
         {sc::kMessagingOperation, "modack"},
         {sc::kCodeFunction, "pubsub::PullLeaseManager::ExtendLease"},
         {"messaging.gcp_pubsub.message.ack_id", child_->ack_id()},
         {"messaging.gcp_pubsub.message.ack_deadline_seconds",
          static_cast<std::int32_t>(extension.count())},
         {"gcp.project_id", child_->subscription().project_id()},
         {sc::kMessagingDestinationName,
          child_->subscription().subscription_id()}},
        CreateLinks(consumer_span_context_), options);
    auto scope = internal::OTelScope(span);
    MaybeAddLinkAttributes(*span, consumer_span_context_, "receive");
    return child_
        ->ExtendLease(std::move(stub), std::move(time_now),
                      std::move(extension))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f) {
          auto result = f.get();
          internal::DetachOTelContext(oc);
          return internal::EndSpan(*span, std::move(result));
        });
  }

  std::string ack_id() const override { return child_->ack_id(); }

  pubsub::Subscription subscription() const override {
    return child_->subscription();
  }

  std::shared_ptr<PullLeaseManager> child_;
  opentelemetry::trace::SpanContext consumer_span_context_;
};

std::shared_ptr<PullLeaseManager> MakeTracingPullLeaseManager(
    std::shared_ptr<PullLeaseManager> manager) {
  return std::make_shared<TracingPullLeaseManager>(std::move(manager));
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::shared_ptr<PullLeaseManager> MakeTracingPullLeaseManager(
    std::shared_ptr<PullLeaseManager> manager) {
  return manager;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

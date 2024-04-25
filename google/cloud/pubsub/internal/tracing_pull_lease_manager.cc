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
#include "google/cloud/pubsub/internal/tracing_helpers.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/internal/async_retry_loop.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/status.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/trace/semantic_conventions.h>
#include <opentelemetry/trace/span.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

class TracingPullLeaseManagerImpl : public PullLeaseManagerImpl {
 public:
  explicit TracingPullLeaseManagerImpl(
      std::shared_ptr<PullLeaseManagerImpl> child, std::string ack_id,
      pubsub::Subscription subscription)
      : child_(std::move(child)),
        ack_id_(std::move(ack_id)),
        subscription_(std::move(subscription)),
        consumer_span_context_(
            opentelemetry::trace::GetSpan(
                opentelemetry::context::RuntimeContext::GetCurrent())
                ->GetContext()) {}

  future<Status> AsyncModifyAckDeadline(
      std::shared_ptr<SubscriberStub> stub, google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::pubsub::v1::ModifyAckDeadlineRequest const& request) override {
    namespace sc = opentelemetry::trace::SemanticConventions;
    opentelemetry::trace::StartSpanOptions start_span_options =
        RootStartSpanOptions();
    start_span_options.kind = opentelemetry::trace::SpanKind::kClient;
    auto span = internal::MakeSpan(
        subscription_.subscription_id() + " modack",
        {{sc::kMessagingSystem, "gcp_pubsub"},
         {sc::kMessagingOperation, "modack"},
         {sc::kCodeFunction, "pubsub::PullLeaseManager::ExtendLease"},
         {"messaging.gcp_pubsub.message.ack_id", ack_id_},
         {"messaging.gcp_pubsub.message.ack_deadline_seconds",
          static_cast<std::int32_t>(request.ack_deadline_seconds())},
         {"gcp.project_id", subscription_.project_id()},
         {sc::kMessagingDestinationName, subscription_.subscription_id()}},
        CreateLinks(consumer_span_context_), start_span_options);
    auto scope = internal::OTelScope(span);
    MaybeAddLinkAttributes(*span, consumer_span_context_, "receive");
    return child_->AsyncModifyAckDeadline(stub, cq, context, options, request)
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               span = std::move(span)](auto f) {
          auto result = f.get();
          internal::DetachOTelContext(oc);
          return internal::EndSpan(*span, std::move(result));
        });
  }

 private:
  std::shared_ptr<PullLeaseManagerImpl> child_;
  std::string ack_id_;
  pubsub::Subscription subscription_;
  opentelemetry::trace::SpanContext consumer_span_context_;
};

std::shared_ptr<PullLeaseManagerImpl> MakeTracingPullLeaseManagerImpl(
    std::shared_ptr<PullLeaseManagerImpl> manager, std::string ack_id,
    pubsub::Subscription subscription) {
  return std::make_shared<TracingPullLeaseManagerImpl>(
      std::move(manager), std::move(ack_id), std::move(subscription));
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::shared_ptr<PullLeaseManagerImpl> MakeTracingPullLeaseManagerImpl(
    std::shared_ptr<PullLeaseManagerImpl> manager, std::string,
    pubsub::Subscription) {
  return manager;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

// Copyright 2022 Google LLC
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

#include "google/cloud/pubsublite/publisher_connection.h"
#include "google/cloud/pubsub/internal/containing_publisher_connection.h"
#include "google/cloud/pubsublite/internal/admin_connection_impl.h"
#include "google/cloud/pubsublite/internal/alarm_registry_impl.h"
#include "google/cloud/pubsublite/internal/batching_options.h"
#include "google/cloud/pubsublite/internal/default_publish_message_transformer.h"
#include "google/cloud/pubsublite/internal/default_routing_policy.h"
#include "google/cloud/pubsublite/internal/location.h"
#include "google/cloud/pubsublite/internal/multipartition_publisher.h"
#include "google/cloud/pubsublite/internal/publisher_connection_impl.h"
#include "google/cloud/pubsublite/internal/publisher_stub_factory.h"
#include "google/cloud/pubsublite/internal/stream_factory.h"
#include "google/cloud/pubsublite/internal/stream_retry_policy.h"
#include "google/cloud/pubsublite/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/base64_transforms.h"
#include "absl/memory/memory.h"
#include <google/protobuf/struct.pb.h>
#include <functional>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using google::cloud::internal::Base64Encoder;
using google::cloud::internal::MakeBackgroundThreadsFactory;
using google::cloud::pubsub::PublisherConnection;
using google::cloud::pubsub_internal::ContainingPublisherConnection;

using google::cloud::pubsublite_internal::AlarmRegistryImpl;
using google::cloud::pubsublite_internal::AsyncSleeper;
using google::cloud::pubsublite_internal::BatchingOptions;
using google::cloud::pubsublite_internal::ClientMetadata;
using google::cloud::pubsublite_internal::CreateDefaultPublisherServiceStub;
using google::cloud::pubsublite_internal::DefaultPublishMessageTransformer;
using google::cloud::pubsublite_internal::DefaultRoutingPolicy;
using google::cloud::pubsublite_internal::MakeLocation;
using google::cloud::pubsublite_internal::MakeStreamFactory;
using google::cloud::pubsublite_internal::MultipartitionPublisher;
using google::cloud::pubsublite_internal::PartitionPublisher;
using google::cloud::pubsublite_internal::PublisherConnectionImpl;
using google::cloud::pubsublite_internal::
    ResumableAsyncStreamingReadWriteRpcImpl;
using google::cloud::pubsublite_internal::RetryPolicyFactory;
using google::cloud::pubsublite_internal::StreamInitializer;
using google::cloud::pubsublite_internal::StreamRetryPolicy;

using google::cloud::pubsublite::v1::InitialPublishRequest;
using google::cloud::pubsublite::v1::PublishRequest;
using google::cloud::pubsublite::v1::PublishResponse;

using google::protobuf::Struct;

BatchingOptions MakeBatchingOptions(Options const& opts) {
  BatchingOptions batching_options;
  if (opts.has<MaxBatchMessagesOption>()) {
    batching_options.set_maximum_batch_message_count(
        opts.get<MaxBatchMessagesOption>());
  }
  if (opts.has<MaxBatchBytesOption>()) {
    batching_options.set_maximum_batch_bytes(opts.get<MaxBatchBytesOption>());
  }
  if (opts.has<PublishFlushAlarmPeriodOption>()) {
    batching_options.set_alarm_period(
        opts.get<PublishFlushAlarmPeriodOption>());
  }
  return batching_options;
}

StatusOr<std::string> GetEndpoint(std::string const& location) {
  auto parsed_loc = MakeLocation(location);
  if (!parsed_loc) return parsed_loc.status();
  return absl::StrCat(parsed_loc->GetCloudRegion().ToString(),
                      "-pubsublite.googleapis.com");
}

std::string GetSerializedContext(std::string const& framework) {
  Struct context;
  auto& metadata_map = *context.mutable_fields();
  metadata_map["language"].set_string_value("CPP");
  metadata_map["framework"].set_string_value(framework);
  Base64Encoder encoder;
  for (unsigned char const c : context.SerializeAsString()) encoder.PushBack(c);
  return std::move(encoder).FlushAndPad();
}

ClientMetadata MakeClientMetadata(Topic const& topic, std::uint32_t partition) {
  ClientMetadata metadata;
  metadata["x-goog-request-params"] =
      absl::StrCat("partition=", partition, "&", "topic=", topic.FullName());
  metadata["x-goog-pubsub-context"] = GetSerializedContext("CLOUD_PUBSUB_SHIM");
  return metadata;
}

StatusOr<std::unique_ptr<PublisherConnection>> MakePublisherConnection(
    Topic topic, Options opts) {
  if (!opts.has<GrpcNumChannelsOption>()) {
    // Each channel has a limit of 100 outstanding RPCs, so 20 allows up to 2000 partitions without reaching this limit
    opts.set<GrpcNumChannelsOption>(20);
  }

  opts = google::cloud::internal::PopulateGrpcOptions(std::move(opts), "");
  if (!opts.has<EndpointOption>()) {
    auto endpoint = GetEndpoint(topic.location_id());
    if (!endpoint) {
      return Status{StatusCode::kInvalidArgument, "`topic` not valid"};
    }
    opts.set<EndpointOption>(*std::move(endpoint));
  }
  opts = google::cloud::internal::PopulateCommonOptions(
      std::move(opts), /*endpoint_env_var=*/{}, /*emulator_env_var=*/{},
      "pubsublite.googleapis.com");

  std::unique_ptr<BackgroundThreads> background_threads =
      MakeBackgroundThreadsFactory(opts)();
  CompletionQueue cq = background_threads->cq();

  // TODO(18suresha): consider supporting `BackoffPolicyOption`
  auto const backoff_policy = std::make_shared<ExponentialBackoffPolicy>(
      std::chrono::milliseconds{10}, std::chrono::seconds{10}, 2.0);
  AsyncSleeper sleeper = [cq](std::chrono::milliseconds backoff_time) mutable {
    return cq.MakeRelativeTimer(backoff_time)
        .then([](future<StatusOr<std::chrono::system_clock::time_point>> f) {
          auto status = f.get();
          if (!status.ok()) {
            GCP_LOG(INFO) << "`MakeRelativeTimer` returned a non-ok status: "
                          << status.status();
          };
        });
  };

  auto publisher_service_stub = CreateDefaultPublisherServiceStub(cq, opts);
  auto batching_options = MakeBatchingOptions(opts);

  auto partition_publisher_factory = [=](std::uint32_t partition) {
    InitialPublishRequest request;
    request.set_topic(topic.FullName());
    request.set_partition(partition);
    AlarmRegistryImpl alarm_registry{cq};
    auto stream_factory = MakeStreamFactory(
        publisher_service_stub, cq, MakeClientMetadata(topic, partition));
    return std::make_shared<PartitionPublisher>(
        [=](StreamInitializer<PublishRequest, PublishResponse> initializer) {
          return absl::make_unique<ResumableAsyncStreamingReadWriteRpcImpl<
              PublishRequest, PublishResponse>>(
              [] { return absl::make_unique<StreamRetryPolicy>(); },
              backoff_policy, sleeper, stream_factory, std::move(initializer));
        },
        batching_options, std::move(request), alarm_registry);
  };

  AlarmRegistryImpl alarm_registry{cq};
  PublishMessageTransformer transformer = &DefaultPublishMessageTransformer;
  if (opts.has<PublishMessageTransformerOption>()) {
    transformer = opts.get<PublishMessageTransformerOption>();
  }

  return std::unique_ptr<PublisherConnection>(
      absl::make_unique<ContainingPublisherConnection>(
          std::move(background_threads),
          absl::make_unique<PublisherConnectionImpl>(
              absl::make_unique<MultipartitionPublisher>(
                  partition_publisher_factory, MakeAdminServiceConnection(opts),
                  alarm_registry, absl::make_unique<DefaultRoutingPolicy>(),
                  std::move(topic)),
              transformer)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google

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
#include "google/cloud/pubsublite/internal/admin_connection_impl.h"
#include "google/cloud/pubsublite/internal/admin_stub_factory.h"
#include "google/cloud/pubsublite/internal/alarm_registry_impl.h"
#include "google/cloud/pubsublite/internal/batching_options.h"
#include "google/cloud/pubsublite/internal/default_routing_policy.h"
#include "google/cloud/pubsublite/internal/location.h"
#include "google/cloud/pubsublite/internal/multipartition_publisher.h"
#include "google/cloud/pubsublite/internal/publisher_connection_impl.h"
#include "google/cloud/pubsublite/internal/publisher_stub_factory.h"
#include "google/cloud/pubsublite/internal/stream_factory.h"
#include "google/cloud/pubsublite/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/base64_transforms.h"
#include "google/cloud/internal/retry_policy.h"
#include "absl/memory/memory.h"
#include <google/protobuf/struct.pb.h>
#include <unordered_set>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using google::cloud::version_major;
using google::cloud::version_minor;

using google::cloud::internal::Base64Encoder;
using google::cloud::internal::MakeBackgroundThreadsFactory;
using google::cloud::internal::RetryPolicy;

using google::cloud::pubsub::Message;
using google::cloud::pubsub::PublisherConnection;

using google::cloud::pubsublite_internal::AlarmRegistryImpl;
using google::cloud::pubsublite_internal::AsyncSleeper;
using google::cloud::pubsublite_internal::BatchingOptions;
using google::cloud::pubsublite_internal::ClientMetadata;
using google::cloud::pubsublite_internal::CreateDefaultAdminServiceStub;
using google::cloud::pubsublite_internal::CreateDefaultPublisherServiceStub;
using google::cloud::pubsublite_internal::DefaultRoutingPolicy;
using google::cloud::pubsublite_internal::MakeLocation;
using google::cloud::pubsublite_internal::MakeStreamFactory;
using google::cloud::pubsublite_internal::MultipartitionPublisher;
using google::cloud::pubsublite_internal::PartitionPublisher;
using google::cloud::pubsublite_internal::Publisher;
using google::cloud::pubsublite_internal::PublisherConnectionImpl;
using google::cloud::pubsublite_internal::
    ResumableAsyncStreamingReadWriteRpcImpl;
using google::cloud::pubsublite_internal::RetryPolicyFactory;
using google::cloud::pubsublite_internal::StreamInitializer;

using google::cloud::pubsublite::v1::AttributeValues;
using google::cloud::pubsublite::v1::InitialPublishRequest;
using google::cloud::pubsublite::v1::PublishRequest;
using google::cloud::pubsublite::v1::PublishResponse;
using google::cloud::pubsublite::v1::PubSubMessage;

using google::protobuf::Struct;
using google::protobuf::Value;

const PublishMessageTransformer kDefaultMessageTransformer =
    [](Message const& message) {
      PubSubMessage m;
      m.set_key(message.ordering_key());
      *m.mutable_data() = message.data();
      for (auto kv : message.attributes()) {
        AttributeValues av;
        av.add_values(std::move(kv.second));
        (*m.mutable_attributes())[kv.first] = std::move(av);
      }
      return m;
    };

class PubSubLiteRetryPolicy : public RetryPolicy {
 public:
  bool OnFailure(Status const& s) override {
    return retryable_codes_.find(static_cast<std::uint8_t>(s.code())) !=
           retryable_codes_.end();
  }

  bool IsExhausted() const override { return false; }

  bool IsPermanentFailure(Status const& s) const override {
    return retryable_codes_.find(static_cast<std::uint8_t>(s.code())) ==
           retryable_codes_.end();
  }

 private:
  std::unordered_set<std::uint8_t> retryable_codes_{
      static_cast<std::uint8_t>(StatusCode::kDeadlineExceeded),
      static_cast<std::uint8_t>(StatusCode::kAborted),
      static_cast<std::uint8_t>(StatusCode::kInternal),
      static_cast<std::uint8_t>(StatusCode::kUnavailable),
      static_cast<std::uint8_t>(StatusCode::kUnknown),
      static_cast<std::uint8_t>(StatusCode::kResourceExhausted)};
};

BatchingOptions CreateBatchingOptions(Options const& opts) {
  BatchingOptions batching_options;
  if (opts.has<MaximumBatchMessageCountOption>()) {
    batching_options.set_maximum_batch_message_count(
        opts.get<MaximumBatchMessageCountOption>());
  }
  if (opts.has<MaximumBatchBytesOption>()) {
    batching_options.set_maximum_batch_bytes(
        opts.get<MaximumBatchBytesOption>());
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

std::string GetSerializedContext() {
  Struct context;
  auto& metadata_map = *context.mutable_fields();
  Value lang;
  *lang.mutable_string_value() = "cpp";
  metadata_map["language"] = std::move(lang);
  Value minor_version;
  minor_version.set_number_value(version_minor());
  Value major_version;
  major_version.set_number_value(version_major());
  std::string serialized_context;
  context.SerializePartialToString(&serialized_context);
  Base64Encoder encoder;
  for (char const c : serialized_context) encoder.PushBack(c);
  return std::move(encoder).FlushAndPad();
}

std::unique_ptr<PublisherConnection> MakePublisherConnection(Topic topic,
                                                             Options opts) {
  if (!opts.has<GrpcNumChannelsOption>()) {
    opts.set<GrpcNumChannelsOption>(20);
  }
  if (!opts.has<PublishMessageTransformerOption>()) {
    opts.set<PublishMessageTransformerOption>(kDefaultMessageTransformer);
  }
  if (!opts.has<FailureHandlerOption>()) {
    opts.set<FailureHandlerOption>([](Status const&) {});
  }
  if (!opts.has<EndpointOption>()) {
    auto endpoint = GetEndpoint(topic.location());
    if (!endpoint) return nullptr;
    opts.set<EndpointOption>(*std::move(endpoint));
  }
  CompletionQueue cq = MakeBackgroundThreadsFactory(opts)()->cq();
  std::shared_ptr<BackoffPolicy const> backoff_policy =
      std::make_shared<ExponentialBackoffPolicy>(std::chrono::milliseconds{10},
                                                 std::chrono::seconds{10}, 2.0);
  AsyncSleeper sleeper = [cq](std::chrono::milliseconds dur) mutable {
    return cq.MakeRelativeTimer(dur).then(
        [](future<StatusOr<std::chrono::system_clock::time_point>> f) {
          auto status = f.get();
          if (!status.ok()) {
            GCP_LOG(INFO) << "`MakeRelativeTimer` returned a non-ok status: "
                          << status.status();
          };
        });
  };
  std::function<std::shared_ptr<
      Publisher<google::cloud::pubsublite::v1::Cursor>>(std::uint32_t)>
      partition_publisher_factory = [topic, backoff_policy, cq, opts,
                                     sleeper](std::uint32_t partition) {
        InitialPublishRequest ipr;
        *ipr.mutable_topic() = topic.FullName();
        ipr.set_partition(partition);
        AlarmRegistryImpl alarm_registry{cq};
        return std::make_shared<PartitionPublisher>(
            [backoff_policy, partition, cq, opts,
             sleeper](StreamInitializer<PublishRequest, PublishResponse>
                          initializer) {
              ClientMetadata metadata;
              metadata["x-goog-request-params"] = absl::StrCat(
                  "partition=", partition, "&",
                  "subscription=subscription");  // TODO(18suresha): figure out
                                                 // subscription path
              metadata["x-goog-pubsub-context"] = GetSerializedContext();
              return absl::make_unique<ResumableAsyncStreamingReadWriteRpcImpl<
                  PublishRequest, PublishResponse>>(
                  []() { return absl::make_unique<PubSubLiteRetryPolicy>(); },
                  backoff_policy, sleeper,
                  MakeStreamFactory(CreateDefaultPublisherServiceStub(cq, opts),
                                    cq, metadata),
                  std::move(initializer));
            },
            CreateBatchingOptions(opts), ipr, alarm_registry);
      };

  AlarmRegistryImpl alarm_registry{cq};
  return absl::make_unique<PublisherConnectionImpl>(
      absl::make_unique<MultipartitionPublisher>(
          partition_publisher_factory,
          MakeAdminServiceConnection(CreateDefaultAdminServiceStub(cq, opts),
                                     opts),
          alarm_registry, absl::make_unique<DefaultRoutingPolicy>(),
          std::move(topic)),
      opts.get<PublishMessageTransformerOption>(),
      opts.get<FailureHandlerOption>());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google

// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_STREAM_FACTORY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_STREAM_FACTORY_H

#include "google/cloud/pubsublite/internal/cursor_stub.h"
#include "google/cloud/pubsublite/internal/partition_assignment_stub.h"
#include "google/cloud/pubsublite/internal/publisher_stub.h"
#include "google/cloud/pubsublite/internal/subscriber_stub.h"
#include "google/cloud/pubsublite/v1/cursor.grpc.pb.h"
#include "google/cloud/pubsublite/v1/publisher.grpc.pb.h"
#include "google/cloud/pubsublite/v1/subscriber.grpc.pb.h"
#include "google/cloud/internal/async_read_write_stream_impl.h"
#include "google/cloud/version.h"
#include <unordered_map>

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <class Request, class Response>
using StreamFactory = std::function<
    std::unique_ptr<AsyncStreamingReadWriteRpc<Request, Response>>()>;

using ClientMetadata = std::unordered_map<std::string, std::string>;

inline std::shared_ptr<grpc::ClientContext> MakeGrpcClientContext(
    ClientMetadata const& metadata) {
  auto context = std::make_shared<grpc::ClientContext>();
  for (auto const& kv : metadata) {
    context->AddMetadata(kv.first, kv.second);
  }
  return context;
}

inline StreamFactory<pubsublite::v1::PublishRequest,
                     pubsublite::v1::PublishResponse>
MakeStreamFactory(std::shared_ptr<PublisherServiceStub> const& stub,
                  google::cloud::CompletionQueue const& cq,
                  ClientMetadata const& metadata) {
  return [=] {
    return stub->AsyncPublish(
        cq, MakeGrpcClientContext(metadata),
        google::cloud::internal::MakeImmutableOptions({}));
  };
}

inline StreamFactory<pubsublite::v1::SubscribeRequest,
                     pubsublite::v1::SubscribeResponse>
MakeStreamFactory(std::shared_ptr<SubscriberServiceStub> const& stub,
                  google::cloud::CompletionQueue const& cq,
                  ClientMetadata const& metadata) {
  return [=] {
    return stub->AsyncSubscribe(
        cq, MakeGrpcClientContext(metadata),
        google::cloud::internal::MakeImmutableOptions({}));
  };
}

inline StreamFactory<pubsublite::v1::StreamingCommitCursorRequest,
                     pubsublite::v1::StreamingCommitCursorResponse>
MakeStreamFactory(std::shared_ptr<CursorServiceStub> const& stub,
                  google::cloud::CompletionQueue const& cq,
                  ClientMetadata const& metadata) {
  return [=] {
    return stub->AsyncStreamingCommitCursor(
        cq, MakeGrpcClientContext(metadata),
        google::cloud::internal::MakeImmutableOptions({}));
  };
}

inline StreamFactory<pubsublite::v1::PartitionAssignmentRequest,
                     pubsublite::v1::PartitionAssignment>
MakeStreamFactory(std::shared_ptr<PartitionAssignmentServiceStub> const& stub,
                  google::cloud::CompletionQueue const& cq,
                  ClientMetadata const& metadata) {
  return [=] {
    return stub->AsyncAssignPartitions(
        cq, MakeGrpcClientContext(metadata),
        google::cloud::internal::MakeImmutableOptions({}));
  };
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_STREAM_FACTORY_H

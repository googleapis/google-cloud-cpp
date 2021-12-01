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

#include <unordered_map>

#include "google/cloud/version.h"
#include "google/cloud/internal/async_read_write_stream_impl.h"

#include <google/cloud/pubsublite/v1/cursor.grpc.pb.h>
#include <google/cloud/pubsublite/v1/publisher.grpc.pb.h>
#include <google/cloud/pubsublite/v1/subscriber.grpc.pb.h>

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <class Request, class Response>
using BidiStream = internal::AsyncStreamingReadWriteRpc<Request, Response>;

template <class Request, class Response>
using StreamFactory = std::function<std::unique_ptr<BidiStream<Request, Response>>()>;

using ClientMetadata = std::unordered_map<std::string, std::string>;

inline std::unique_ptr<grpc::ClientContext> MakeGrpcClientContext(const ClientMetadata& metadata) {
    auto context = absl::make_unique<grpc::ClientContext>();
    for (const auto& kv : metadata) {
        context->AddMetadata(kv.first, kv.second);
    }
    return context;
}

inline StreamFactory<pubsublite::v1::PublishRequest, pubsublite::v1::PublishResponse> MakeStreamFactory(
    std::shared_ptr<pubsublite::v1::PublisherService::StubInterface> stub,
    const google::cloud::CompletionQueue& cq, ClientMetadata metadata = {}) {
        return [=]{
            return internal::MakeStreamingReadWriteRpc<
                pubsublite::v1::PublishRequest, pubsublite::v1::PublishResponse>(
                    cq, MakeGrpcClientContext(metadata),
                    [stub](grpc::ClientContext* context, grpc::CompletionQueue* cq) {
                        return stub->PrepareAsyncPublish(context, cq);
                    });
        };
    }

inline StreamFactory<pubsublite::v1::SubscribeRequest, pubsublite::v1::SubscribeResponse> MakeStreamFactory(
    std::shared_ptr<pubsublite::v1::SubscriberService::StubInterface> stub,
    const google::cloud::CompletionQueue& cq, ClientMetadata metadata = {}) {
        return [=]{
            return google::cloud::internal::MakeStreamingReadWriteRpc<
                pubsublite::v1::SubscribeRequest, pubsublite::v1::SubscribeResponse>(
                    cq, MakeGrpcClientContext(metadata),
                    [stub](grpc::ClientContext* context, grpc::CompletionQueue* cq) {
                        return stub->PrepareAsyncSubscribe(context, cq);
                    });
        };
    }



inline StreamFactory<pubsublite::v1::StreamingCommitCursorRequest, pubsublite::v1::StreamingCommitCursorResponse> MakeStreamFactory(
    std::shared_ptr<pubsublite::v1::CursorService::StubInterface> stub,
    const google::cloud::CompletionQueue& cq, ClientMetadata metadata = {}) {
        return [=]{
            return google::cloud::internal::MakeStreamingReadWriteRpc<
                pubsublite::v1::StreamingCommitCursorRequest, pubsublite::v1::StreamingCommitCursorResponse>(
                    cq, MakeGrpcClientContext(metadata),
                    [stub](grpc::ClientContext* context, grpc::CompletionQueue* cq) {
                        return stub->PrepareAsyncStreamingCommitCursor(context, cq);
                    });
        };
    }


inline StreamFactory<pubsublite::v1::PartitionAssignmentRequest, pubsublite::v1::PartitionAssignment> MakeStreamFactory(
    std::shared_ptr<pubsublite::v1::PartitionAssignmentService::StubInterface> stub,
    const google::cloud::CompletionQueue& cq, ClientMetadata metadata = {}) {
        return [=]{
            return google::cloud::internal::MakeStreamingReadWriteRpc<
                pubsublite::v1::PartitionAssignmentRequest, pubsublite::v1::PartitionAssignment>(
                    cq, MakeGrpcClientContext(metadata),
                    [stub](grpc::ClientContext* context, grpc::CompletionQueue* cq) {
                        return stub->PrepareAsyncAssignPartitions(context, cq);
                    });
        };
    }

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_STREAM_FACTORY_H

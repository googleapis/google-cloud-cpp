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

#include "google/cloud/internal/async_read_write_stream_auth.h"
#include "google/cloud/internal/async_read_write_stream_impl.h"
#include "google/cloud/version.h"
#include <google/cloud/pubsublite/v1/cursor.grpc.pb.h>
#include <google/cloud/pubsublite/v1/publisher.grpc.pb.h>
#include <google/cloud/pubsublite/v1/subscriber.grpc.pb.h>
#include <unordered_map>

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <class Request, class Response>
using BidiStream = internal::AsyncStreamingReadWriteRpc<Request, Response>;

template <class Request, class Response>
using StreamFactory =
    std::function<std::unique_ptr<BidiStream<Request, Response>>()>;

template <class Request, class Response>
using ContextStreamFactory =
    std::function<std::unique_ptr<BidiStream<Request, Response>>(
        std::unique_ptr<grpc::ClientContext> context)>;

using ClientMetadata = std::unordered_map<std::string, std::string>;

inline std::unique_ptr<grpc::ClientContext> MakeGrpcClientContext(
    ClientMetadata const& metadata) {
  auto context = absl::make_unique<grpc::ClientContext>();
  for (auto const& kv : metadata) {
    context->AddMetadata(kv.first, kv.second);
  }
  return context;
}

template <class Request, class Response>
StreamFactory<Request, Response> AddClientMetadata(
    ClientMetadata const& metadata,
    ContextStreamFactory<Request, Response> underlying) {
  return [=] { return underlying(MakeGrpcClientContext(metadata)); };
}

template <typename Request, typename Response>
inline ContextStreamFactory<Request, Response> AddAuthContext(
    std::shared_ptr<google::cloud::internal::GrpcAuthenticationStrategy> const&
        auth,
    ContextStreamFactory<Request, Response> const& underlying) {
  return [=](std::unique_ptr<grpc::ClientContext> context) {
    return absl::make_unique<
        internal::AsyncStreamingReadWriteRpcAuth<Request, Response>>(
        std::move(context), auth,
        [=](std::unique_ptr<grpc::ClientContext> ctx) {
          return underlying(std::move(ctx));
        });
  };
}

template <class Obj, class R, class... Args>
using MemFn = R (Obj::*)(Args...);

template <class Stub, class Request, class Response>
inline ContextStreamFactory<Request, Response> MakeRawStreamFactory(
    google::cloud::CompletionQueue const& cq, std::shared_ptr<Stub> const& stub,
    MemFn<Stub,
          std::unique_ptr<
              grpc::ClientAsyncReaderWriterInterface<Request, Response>>,
          grpc::ClientContext*, grpc::CompletionQueue*>
        new_stream) {
  return [=](std::unique_ptr<grpc::ClientContext> context)
             -> std::unique_ptr<BidiStream<Request, Response>> {
    return internal::MakeStreamingReadWriteRpc<Request, Response>(
        cq, std::move(context), absl::bind_front(new_stream, stub));
  };
}

inline StreamFactory<pubsublite::v1::PublishRequest,
                     pubsublite::v1::PublishResponse>
MakeStreamFactory(
    std::shared_ptr<pubsublite::v1::PublisherService::StubInterface> const&
        stub,
    google::cloud::CompletionQueue const& cq,
    std::shared_ptr<google::cloud::internal::GrpcAuthenticationStrategy> const&
        auth,
    ClientMetadata const& metadata = {}) {
  return AddClientMetadata(
      metadata,
      AddAuthContext(
          auth, MakeRawStreamFactory(cq, stub,
                                     &pubsublite::v1::PublisherService::
                                         StubInterface::PrepareAsyncPublish)));
}

inline StreamFactory<pubsublite::v1::SubscribeRequest,
                     pubsublite::v1::SubscribeResponse>
MakeStreamFactory(
    std::shared_ptr<pubsublite::v1::SubscriberService::StubInterface> const&
        stub,
    google::cloud::CompletionQueue const& cq,
    std::shared_ptr<google::cloud::internal::GrpcAuthenticationStrategy> const&
        auth,
    ClientMetadata const& metadata = {}) {
  return AddClientMetadata(
      metadata,
      AddAuthContext(auth, MakeRawStreamFactory(
                               cq, stub,
                               &pubsublite::v1::SubscriberService::
                                   StubInterface::PrepareAsyncSubscribe)));
}

inline StreamFactory<pubsublite::v1::StreamingCommitCursorRequest,
                     pubsublite::v1::StreamingCommitCursorResponse>
MakeStreamFactory(
    std::shared_ptr<pubsublite::v1::CursorService::StubInterface> const& stub,
    google::cloud::CompletionQueue const& cq,
    std::shared_ptr<google::cloud::internal::GrpcAuthenticationStrategy> auth,
    ClientMetadata const& metadata = {}) {
  return AddClientMetadata(
      metadata,
      AddAuthContext(auth, MakeRawStreamFactory(
                               cq, stub,
                               &pubsublite::v1::CursorService::StubInterface::
                                   PrepareAsyncStreamingCommitCursor)));
}

inline StreamFactory<pubsublite::v1::PartitionAssignmentRequest,
                     pubsublite::v1::PartitionAssignment>
MakeStreamFactory(
    std::shared_ptr<
        pubsublite::v1::PartitionAssignmentService::StubInterface> const& stub,
    google::cloud::CompletionQueue const& cq,
    std::shared_ptr<google::cloud::internal::GrpcAuthenticationStrategy> const&
        auth,
    ClientMetadata const& metadata = {}) {
  return AddClientMetadata(
      metadata,
      AddAuthContext(
          auth, MakeRawStreamFactory(
                    cq, stub,
                    &pubsublite::v1::PartitionAssignmentService::StubInterface::
                        PrepareAsyncAssignPartitions)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_STREAM_FACTORY_H

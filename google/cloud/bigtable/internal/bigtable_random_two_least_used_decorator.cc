// Copyright 2026 Google LLC
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

#include "google/cloud/bigtable/internal/bigtable_random_two_least_used_decorator.h"
#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/bigtable/internal/operation_context.h"
#include "google/cloud/async_streaming_read_write_rpc.h"
#include "google/cloud/internal/async_streaming_read_rpc.h"
#include "google/cloud/internal/streaming_read_rpc.h"
#include "google/cloud/status_or.h"
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

template <typename T>
class StreamingReadRpcTracking
    : public google::cloud::internal::StreamingReadRpc<T> {
 public:
  StreamingReadRpcTracking(
      std::unique_ptr<google::cloud::internal::StreamingReadRpc<T>> child,
      std::function<void(void)> on_destruction)
      : child_(std::move(child)), on_destruction_(std::move(on_destruction)) {}

  ~StreamingReadRpcTracking() override { on_destruction_(); }

  void Cancel() override { child_->Cancel(); }
  std::optional<Status> Read(T* response) override {
    return child_->Read(response);
  }
  RpcMetadata GetRequestMetadata() const override {
    return child_->GetRequestMetadata();
  }

 private:
  std::unique_ptr<google::cloud::internal::StreamingReadRpc<T>> child_;
  std::function<void(void)> on_destruction_;
};

template <typename T>
class AsyncStreamingReadRpcTracking
    : public google::cloud::internal::AsyncStreamingReadRpc<T> {
 public:
  AsyncStreamingReadRpcTracking(
      std::unique_ptr<google::cloud::internal::AsyncStreamingReadRpc<T>> child,
      std::function<void(void)> on_destruction)
      : child_(std::move(child)), on_destruction_(std::move(on_destruction)) {}

  ~AsyncStreamingReadRpcTracking() override { on_destruction_(); }

  void Cancel() override { child_->Cancel(); }
  future<bool> Start() override { return child_->Start(); }
  future<std::optional<T>> Read() override { return child_->Read(); }
  future<Status> Finish() override { return child_->Finish(); }
  RpcMetadata GetRequestMetadata() const override {
    return child_->GetRequestMetadata();
  }

 private:
  std::unique_ptr<google::cloud::internal::AsyncStreamingReadRpc<T>> child_;
  std::function<void(void)> on_destruction_;
};

template <typename Request, typename Response>
class AsyncStreamingReadWriteRpcTracking
    : public google::cloud::AsyncStreamingReadWriteRpc<Request, Response> {
 public:
  AsyncStreamingReadWriteRpcTracking(
      std::unique_ptr<
          google::cloud::AsyncStreamingReadWriteRpc<Request, Response>>
          child,
      std::function<void(void)> on_destruction)
      : child_(std::move(child)), on_destruction_(std::move(on_destruction)) {}

  ~AsyncStreamingReadWriteRpcTracking() override { on_destruction_(); }

  void Cancel() override { child_->Cancel(); }
  future<bool> Start() override { return child_->Start(); }
  future<std::optional<Response>> Read() override { return child_->Read(); }
  future<bool> Write(Request const& r, grpc::WriteOptions o) override {
    return child_->Write(r, std::move(o));
  }
  future<bool> WritesDone() override { return child_->WritesDone(); }
  future<Status> Finish() override { return child_->Finish(); }
  RpcMetadata GetRequestMetadata() const override {
    return child_->GetRequestMetadata();
  }

 private:
  std::unique_ptr<google::cloud::AsyncStreamingReadWriteRpc<Request, Response>>
      child_;
  std::function<void(void)> on_destruction_;
};

template <typename Response>
Response UnaryHelper(std::shared_ptr<DynamicChannelPool<BigtableStub>>& pool,
                     OperationContext& oc,
                     std::function<Response(BigtableStub&)> fn) {
  auto selection = pool->GetChannelRandomTwoLeastUsed();
  oc.StubSelection(StubSelectionParams{
      selection.outstanding_rpcs, ChannelPoolLbPolicy::kRandomTwoLeastUsed,
      pool->transport_type(), RpcType::kUnary});
  auto stub = selection.channel->AcquireStub();
  auto result = fn(*stub);
  selection.channel->ReleaseStub();
  return result;
}

template <typename Response>
std::unique_ptr<google::cloud::internal::StreamingReadRpc<Response>>
StreamingHelper(
    std::shared_ptr<DynamicChannelPool<BigtableStub>>& pool,
    OperationContext& oc,
    std::function<std::unique_ptr<
        google::cloud::internal::StreamingReadRpc<Response>>(BigtableStub&)>
        fn) {
  auto selection = pool->GetChannelRandomTwoLeastUsed();
  oc.StubSelection(StubSelectionParams{
      selection.outstanding_rpcs, ChannelPoolLbPolicy::kRandomTwoLeastUsed,
      pool->transport_type(), RpcType::kStreaming});
  auto stub = selection.channel->AcquireStub();
  auto result = fn(*stub);
  auto release_fn = [weak = selection.channel->MakeWeak()] {
    auto child = weak.lock();
    if (child) child->ReleaseStub();
  };
  return std::make_unique<StreamingReadRpcTracking<Response>>(
      std::move(result), std::move(release_fn));
}

template <typename Response>
std::unique_ptr<google::cloud::internal::AsyncStreamingReadRpc<Response>>
AsyncStreamingHelper(
    std::shared_ptr<DynamicChannelPool<BigtableStub>>& pool,
    OperationContext& oc,
    std::function<std::unique_ptr<
        google::cloud::internal::AsyncStreamingReadRpc<Response>>(
        BigtableStub&)>
        fn) {
  auto selection = pool->GetChannelRandomTwoLeastUsed();
  oc.StubSelection(StubSelectionParams{
      selection.outstanding_rpcs, ChannelPoolLbPolicy::kRandomTwoLeastUsed,
      pool->transport_type(), RpcType::kStreaming});
  auto stub = selection.channel->AcquireStub();
  auto result = fn(*stub);
  auto release_fn = [weak = selection.channel->MakeWeak()] {
    auto child = weak.lock();
    if (child) child->ReleaseStub();
  };
  return std::make_unique<AsyncStreamingReadRpcTracking<Response>>(
      std::move(result), std::move(release_fn));
}

template <typename Request, typename Response>
std::unique_ptr<google::cloud::AsyncStreamingReadWriteRpc<Request, Response>>
AsyncStreamingHelper(
    std::shared_ptr<DynamicChannelPool<BigtableStub>>& pool,
    std::function<std::unique_ptr<google::cloud::AsyncStreamingReadWriteRpc<
        Request, Response>>(BigtableStub&)>
        fn) {
  auto selection = pool->GetChannelRandomTwoLeastUsed();
  auto stub = selection.channel->AcquireStub();
  auto result = fn(*stub);
  auto release_fn = [weak = selection.channel->MakeWeak()] {
    auto child = weak.lock();
    if (child) child->ReleaseStub();
  };
  return std::make_unique<
      AsyncStreamingReadWriteRpcTracking<Request, Response>>(
      std::move(result), std::move(release_fn));
}

}  // namespace

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::ReadRowsResponse>>
BigtableRandomTwoLeastUsed::ReadRows(
    std::shared_ptr<grpc::ClientContext> context, Options const& options,
    google::bigtable::v2::ReadRowsRequest const& request,
    OperationContext& oc) {
  return StreamingHelper<google::bigtable::v2::ReadRowsResponse>(
      pool_, oc, [&, context = std::move(context)](BigtableStub& stub) mutable {
        return stub.ReadRows(std::move(context), options, request, oc);
      });
}

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::SampleRowKeysResponse>>
BigtableRandomTwoLeastUsed::SampleRowKeys(
    std::shared_ptr<grpc::ClientContext> context, Options const& options,
    google::bigtable::v2::SampleRowKeysRequest const& request,
    OperationContext& oc) {
  return StreamingHelper<google::bigtable::v2::SampleRowKeysResponse>(
      pool_, oc, [&, context = std::move(context)](BigtableStub& stub) mutable {
        return stub.SampleRowKeys(std::move(context), options, request, oc);
      });
}

StatusOr<google::bigtable::v2::MutateRowResponse>
BigtableRandomTwoLeastUsed::MutateRow(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::MutateRowRequest const& request,
    OperationContext& oc) {
  return UnaryHelper<StatusOr<google::bigtable::v2::MutateRowResponse>>(
      pool_, oc, [&](BigtableStub& stub) {
        return stub.MutateRow(context, options, request, oc);
      });
}

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::MutateRowsResponse>>
BigtableRandomTwoLeastUsed::MutateRows(
    std::shared_ptr<grpc::ClientContext> context, Options const& options,
    google::bigtable::v2::MutateRowsRequest const& request,
    OperationContext& oc) {
  return StreamingHelper<google::bigtable::v2::MutateRowsResponse>(
      pool_, oc, [&, context = std::move(context)](BigtableStub& stub) mutable {
        return stub.MutateRows(std::move(context), options, request, oc);
      });
}

StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>
BigtableRandomTwoLeastUsed::CheckAndMutateRow(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::CheckAndMutateRowRequest const& request,
    OperationContext& oc) {
  return UnaryHelper<StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>>(
      pool_, oc, [&](BigtableStub& stub) {
        return stub.CheckAndMutateRow(context, options, request, oc);
      });
}

StatusOr<google::bigtable::v2::PingAndWarmResponse>
BigtableRandomTwoLeastUsed::PingAndWarm(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::PingAndWarmRequest const& request,
    OperationContext& oc) {
  return UnaryHelper<StatusOr<google::bigtable::v2::PingAndWarmResponse>>(
      pool_, oc, [&](BigtableStub& stub) {
        return stub.PingAndWarm(context, options, request, oc);
      });
}

StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>
BigtableRandomTwoLeastUsed::ReadModifyWriteRow(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::ReadModifyWriteRowRequest const& request,
    OperationContext& oc) {
  return UnaryHelper<
      StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>>(
      pool_, oc, [&](BigtableStub& stub) {
        return stub.ReadModifyWriteRow(context, options, request, oc);
      });
}

StatusOr<google::bigtable::v2::PrepareQueryResponse>
BigtableRandomTwoLeastUsed::PrepareQuery(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::PrepareQueryRequest const& request,
    OperationContext& oc) {
  return UnaryHelper<StatusOr<google::bigtable::v2::PrepareQueryResponse>>(
      pool_, oc, [&](BigtableStub& stub) {
        return stub.PrepareQuery(context, options, request, oc);
      });
}

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::ExecuteQueryResponse>>
BigtableRandomTwoLeastUsed::ExecuteQuery(
    std::shared_ptr<grpc::ClientContext> context, Options const& options,
    google::bigtable::v2::ExecuteQueryRequest const& request,
    OperationContext& oc) {
  return StreamingHelper<google::bigtable::v2::ExecuteQueryResponse>(
      pool_, oc, [&, context = std::move(context)](BigtableStub& stub) mutable {
        return stub.ExecuteQuery(std::move(context), options, request, oc);
      });
}

std::unique_ptr<google::cloud::internal::AsyncStreamingReadRpc<
    google::bigtable::v2::ReadRowsResponse>>
BigtableRandomTwoLeastUsed::AsyncReadRows(
    google::cloud::CompletionQueue const& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::bigtable::v2::ReadRowsRequest const& request,
    OperationContext& oc) {
  return AsyncStreamingHelper<google::bigtable::v2::ReadRowsResponse>(
      pool_, oc,
      [&, context = std::move(context),
       options = std::move(options)](BigtableStub& stub) mutable {
        return stub.AsyncReadRows(cq, std::move(context), std::move(options),
                                  request, oc);
      });
}

std::unique_ptr<google::cloud::internal::AsyncStreamingReadRpc<
    google::bigtable::v2::SampleRowKeysResponse>>
BigtableRandomTwoLeastUsed::AsyncSampleRowKeys(
    google::cloud::CompletionQueue const& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::bigtable::v2::SampleRowKeysRequest const& request,
    OperationContext& oc) {
  return AsyncStreamingHelper<google::bigtable::v2::SampleRowKeysResponse>(
      pool_, oc,
      [&, context = std::move(context),
       options = std::move(options)](BigtableStub& stub) mutable {
        return stub.AsyncSampleRowKeys(cq, std::move(context),
                                       std::move(options), request, oc);
      });
}

future<StatusOr<google::bigtable::v2::MutateRowResponse>>
BigtableRandomTwoLeastUsed::AsyncMutateRow(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::bigtable::v2::MutateRowRequest const& request,
    OperationContext& oc) {
  return UnaryHelper<future<StatusOr<google::bigtable::v2::MutateRowResponse>>>(
      pool_, oc,
      [&, context = std::move(context),
       options = std::move(options)](BigtableStub& stub) mutable {
        return stub.AsyncMutateRow(cq, std::move(context), std::move(options),
                                   request, oc);
      });
}

std::unique_ptr<google::cloud::internal::AsyncStreamingReadRpc<
    google::bigtable::v2::MutateRowsResponse>>
BigtableRandomTwoLeastUsed::AsyncMutateRows(
    google::cloud::CompletionQueue const& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::bigtable::v2::MutateRowsRequest const& request,
    OperationContext& oc) {
  return AsyncStreamingHelper<google::bigtable::v2::MutateRowsResponse>(
      pool_, oc,
      [&, context = std::move(context),
       options = std::move(options)](BigtableStub& stub) mutable {
        return stub.AsyncMutateRows(cq, std::move(context), std::move(options),
                                    request, oc);
      });
}

future<StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>>
BigtableRandomTwoLeastUsed::AsyncCheckAndMutateRow(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::bigtable::v2::CheckAndMutateRowRequest const& request,
    OperationContext& oc) {
  return UnaryHelper<
      future<StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>>>(
      pool_, oc,
      [&, context = std::move(context),
       options = std::move(options)](BigtableStub& stub) mutable {
        return stub.AsyncCheckAndMutateRow(cq, std::move(context),
                                           std::move(options), request, oc);
      });
}

future<StatusOr<google::bigtable::v2::PingAndWarmResponse>>
BigtableRandomTwoLeastUsed::AsyncPingAndWarm(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::bigtable::v2::PingAndWarmRequest const& request,
    OperationContext& oc) {
  return UnaryHelper<
      future<StatusOr<google::bigtable::v2::PingAndWarmResponse>>>(
      pool_, oc,
      [&, context = std::move(context),
       options = std::move(options)](BigtableStub& stub) mutable {
        return stub.AsyncPingAndWarm(cq, std::move(context), std::move(options),
                                     request, oc);
      });
}

future<StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>>
BigtableRandomTwoLeastUsed::AsyncReadModifyWriteRow(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::bigtable::v2::ReadModifyWriteRowRequest const& request,
    OperationContext& oc) {
  return UnaryHelper<
      future<StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>>>(
      pool_, oc,
      [&, context = std::move(context),
       options = std::move(options)](BigtableStub& stub) mutable {
        return stub.AsyncReadModifyWriteRow(cq, std::move(context),
                                            std::move(options), request, oc);
      });
}

future<StatusOr<google::bigtable::v2::PrepareQueryResponse>>
BigtableRandomTwoLeastUsed::AsyncPrepareQuery(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::bigtable::v2::PrepareQueryRequest const& request,
    OperationContext& oc) {
  return UnaryHelper<
      future<StatusOr<google::bigtable::v2::PrepareQueryResponse>>>(
      pool_, oc,
      [&, context = std::move(context),
       options = std::move(options)](BigtableStub& stub) mutable {
        return stub.AsyncPrepareQuery(cq, std::move(context),
                                      std::move(options), request, oc);
      });
}

StatusOr<google::bigtable::v2::ClientConfiguration>
BigtableRandomTwoLeastUsed::GetClientConfiguration(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::GetClientConfigurationRequest const& request,
    OperationContext& oc) {
  return UnaryHelper<StatusOr<google::bigtable::v2::ClientConfiguration>>(
      pool_, oc, [&](BigtableStub& stub) {
        return stub.GetClientConfiguration(context, options, request, oc);
      });
}

std::unique_ptr<::google::cloud::AsyncStreamingReadWriteRpc<
    google::bigtable::v2::SessionRequest,
    google::bigtable::v2::SessionResponse>>
BigtableRandomTwoLeastUsed::AsyncOpenTable(
    google::cloud::CompletionQueue const& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options) {
  return AsyncStreamingHelper<google::bigtable::v2::SessionRequest,
                              google::bigtable::v2::SessionResponse>(
      pool_, [&, context = std::move(context),
              options = std::move(options)](BigtableStub& stub) mutable {
        return stub.AsyncOpenTable(cq, std::move(context), std::move(options));
      });
}

std::unique_ptr<::google::cloud::AsyncStreamingReadWriteRpc<
    google::bigtable::v2::SessionRequest,
    google::bigtable::v2::SessionResponse>>
BigtableRandomTwoLeastUsed::AsyncOpenAuthorizedView(
    google::cloud::CompletionQueue const& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options) {
  return AsyncStreamingHelper<google::bigtable::v2::SessionRequest,
                              google::bigtable::v2::SessionResponse>(
      pool_, [&, context = std::move(context),
              options = std::move(options)](BigtableStub& stub) mutable {
        return stub.AsyncOpenAuthorizedView(cq, std::move(context),
                                            std::move(options));
      });
}

std::unique_ptr<::google::cloud::AsyncStreamingReadWriteRpc<
    google::bigtable::v2::SessionRequest,
    google::bigtable::v2::SessionResponse>>
BigtableRandomTwoLeastUsed::AsyncOpenMaterializedView(
    google::cloud::CompletionQueue const& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options) {
  return AsyncStreamingHelper<google::bigtable::v2::SessionRequest,
                              google::bigtable::v2::SessionResponse>(
      pool_, [&, context = std::move(context),
              options = std::move(options)](BigtableStub& stub) mutable {
        return stub.AsyncOpenMaterializedView(cq, std::move(context),
                                              std::move(options));
      });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

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
#include "google/cloud/async_streaming_read_write_rpc.h"
#include "google/cloud/internal/async_streaming_read_rpc.h"
#include "google/cloud/internal/streaming_read_rpc.h"
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
class StreamingReadRpcTracking : public internal::StreamingReadRpc<T> {
 public:
  StreamingReadRpcTracking(std::unique_ptr<internal::StreamingReadRpc<T>> child,
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
  std::unique_ptr<internal::StreamingReadRpc<T>> child_;
  std::function<void(void)> on_destruction_;
};

template <typename T>
class AsyncStreamingReadRpcTracking
    : public internal::AsyncStreamingReadRpc<T> {
 public:
  AsyncStreamingReadRpcTracking(
      std::unique_ptr<internal::AsyncStreamingReadRpc<T>> child,
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
  std::unique_ptr<internal::AsyncStreamingReadRpc<T>> child_;
  std::function<void(void)> on_destruction_;
};

template <typename Request, typename Response>
class AsyncStreamingReadWriteRpcTracking
    : public AsyncStreamingReadWriteRpc<Request, Response> {
 public:
  AsyncStreamingReadWriteRpcTracking(
      std::unique_ptr<AsyncStreamingReadWriteRpc<Request, Response>> child,
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
  std::unique_ptr<AsyncStreamingReadWriteRpc<Request, Response>> child_;
  std::function<void(void)> on_destruction_;
};

template <typename Response>
Response UnaryHelper(std::shared_ptr<DynamicChannelPool<BigtableStub>>& pool,
                     OperationContext& oc,
                     std::function<Response(BigtableStub&)> fn) {
  SelectedChannel<BigtableStub> selection =
      pool->GetChannelRandomTwoLeastUsed();
  oc.StubSelection(StubSelectionParams{
      selection.outstanding_rpcs, ChannelPoolLbPolicy::kRandomTwoLeastUsed,
      pool->transport_type(), RpcType::kUnary});
  std::shared_ptr<BigtableStub> stub = selection.channel->AcquireStub();
  Response result = fn(*stub);
  selection.channel->ReleaseStub();
  return result;
}

template <typename Response>
Response AsyncHelper(std::shared_ptr<DynamicChannelPool<BigtableStub>>& pool,
                     std::shared_ptr<OperationContext> const& operation_context,
                     std::function<Response(BigtableStub&)> fn) {
  SelectedChannel<BigtableStub> selection =
      pool->GetChannelRandomTwoLeastUsed();
  if (operation_context != nullptr) {
    operation_context->StubSelection(StubSelectionParams{
        selection.outstanding_rpcs, ChannelPoolLbPolicy::kRandomTwoLeastUsed,
        pool->transport_type(), RpcType::kUnary});
  }
  std::shared_ptr<BigtableStub> stub = selection.channel->AcquireStub();
  Response result = fn(*stub);
  selection.channel->ReleaseStub();
  return result;
}

template <typename Response>
std::unique_ptr<internal::StreamingReadRpc<Response>> StreamingHelper(
    std::shared_ptr<DynamicChannelPool<BigtableStub>>& pool,
    std::shared_ptr<OperationContext> const& operation_context,
    std::function<
        std::unique_ptr<internal::StreamingReadRpc<Response>>(BigtableStub&)>
        fn) {
  SelectedChannel<BigtableStub> selection =
      pool->GetChannelRandomTwoLeastUsed();
  if (operation_context != nullptr) {
    operation_context->StubSelection(StubSelectionParams{
        selection.outstanding_rpcs, ChannelPoolLbPolicy::kRandomTwoLeastUsed,
        pool->transport_type(), RpcType::kStreaming});
  }
  std::shared_ptr<BigtableStub> stub = selection.channel->AcquireStub();
  std::unique_ptr<internal::StreamingReadRpc<Response>> result = fn(*stub);
  auto release_fn = [weak = selection.channel->MakeWeak()] {
    std::shared_ptr<ChannelUsage<BigtableStub>> child = weak.lock();
    if (child) child->ReleaseStub();
  };
  return std::make_unique<StreamingReadRpcTracking<Response>>(
      std::move(result), std::move(release_fn));
}

template <typename Response>
std::unique_ptr<internal::AsyncStreamingReadRpc<Response>> AsyncStreamingHelper(
    std::shared_ptr<DynamicChannelPool<BigtableStub>>& pool,
    std::shared_ptr<OperationContext> const& operation_context,
    std::function<std::unique_ptr<internal::AsyncStreamingReadRpc<Response>>(
        BigtableStub&)>
        fn) {
  SelectedChannel<BigtableStub> selection =
      pool->GetChannelRandomTwoLeastUsed();
  if (operation_context != nullptr) {
    operation_context->StubSelection(StubSelectionParams{
        selection.outstanding_rpcs, ChannelPoolLbPolicy::kRandomTwoLeastUsed,
        pool->transport_type(), RpcType::kStreaming});
  }
  std::shared_ptr<BigtableStub> stub = selection.channel->AcquireStub();
  std::unique_ptr<internal::AsyncStreamingReadRpc<Response>> result = fn(*stub);
  auto release_fn = [weak = selection.channel->MakeWeak()] {
    std::shared_ptr<ChannelUsage<BigtableStub>> child = weak.lock();
    if (child) child->ReleaseStub();
  };
  return std::make_unique<AsyncStreamingReadRpcTracking<Response>>(
      std::move(result), std::move(release_fn));
}

template <typename Request, typename Response>
std::unique_ptr<AsyncStreamingReadWriteRpc<Request, Response>>
AsyncStreamingHelper(
    std::shared_ptr<DynamicChannelPool<BigtableStub>>& pool,
    std::shared_ptr<OperationContext> const& operation_context,
    std::function<std::unique_ptr<
        AsyncStreamingReadWriteRpc<Request, Response>>(BigtableStub&)>
        fn) {
  SelectedChannel<BigtableStub> selection =
      pool->GetChannelRandomTwoLeastUsed();
  if (operation_context != nullptr) {
    operation_context->StubSelection(StubSelectionParams{
        selection.outstanding_rpcs, ChannelPoolLbPolicy::kRandomTwoLeastUsed,
        pool->transport_type(), RpcType::kStreaming});
  }
  std::shared_ptr<BigtableStub> stub = selection.channel->AcquireStub();
  std::unique_ptr<AsyncStreamingReadWriteRpc<Request, Response>> result =
      fn(*stub);
  auto release_fn = [weak = selection.channel->MakeWeak()] {
    std::shared_ptr<ChannelUsage<BigtableStub>> child = weak.lock();
    if (child) child->ReleaseStub();
  };
  return std::make_unique<
      AsyncStreamingReadWriteRpcTracking<Request, Response>>(
      std::move(result), std::move(release_fn));
}

}  // namespace

std::unique_ptr<
    internal::StreamingReadRpc<google::bigtable::v2::ReadRowsResponse>>
BigtableRandomTwoLeastUsed::ReadRows(
    std::shared_ptr<grpc::ClientContext> context, Options const& options,
    google::bigtable::v2::ReadRowsRequest const& request,
    std::shared_ptr<OperationContext> operation_context) {
  return StreamingHelper<google::bigtable::v2::ReadRowsResponse>(
      pool_, operation_context,
      [&, context = std::move(context),
       operation_context](BigtableStub& stub) mutable {
        return stub.ReadRows(std::move(context), options, request,
                             std::move(operation_context));
      });
}

std::unique_ptr<
    internal::StreamingReadRpc<google::bigtable::v2::SampleRowKeysResponse>>
BigtableRandomTwoLeastUsed::SampleRowKeys(
    std::shared_ptr<grpc::ClientContext> context, Options const& options,
    google::bigtable::v2::SampleRowKeysRequest const& request,
    std::shared_ptr<OperationContext> operation_context) {
  return StreamingHelper<google::bigtable::v2::SampleRowKeysResponse>(
      pool_, operation_context,
      [&, context = std::move(context),
       operation_context](BigtableStub& stub) mutable {
        return stub.SampleRowKeys(std::move(context), options, request,
                                  std::move(operation_context));
      });
}

StatusOr<google::bigtable::v2::MutateRowResponse>
BigtableRandomTwoLeastUsed::MutateRow(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::MutateRowRequest const& request,
    OperationContext& operation_context) {
  return UnaryHelper<StatusOr<google::bigtable::v2::MutateRowResponse>>(
      pool_, operation_context, [&](BigtableStub& stub) {
        return stub.MutateRow(context, options, request, operation_context);
      });
}

std::unique_ptr<
    internal::StreamingReadRpc<google::bigtable::v2::MutateRowsResponse>>
BigtableRandomTwoLeastUsed::MutateRows(
    std::shared_ptr<grpc::ClientContext> context, Options const& options,
    google::bigtable::v2::MutateRowsRequest const& request,
    std::shared_ptr<OperationContext> operation_context) {
  return StreamingHelper<google::bigtable::v2::MutateRowsResponse>(
      pool_, operation_context,
      [&, context = std::move(context),
       operation_context](BigtableStub& stub) mutable {
        return stub.MutateRows(std::move(context), options, request,
                               std::move(operation_context));
      });
}

StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>
BigtableRandomTwoLeastUsed::CheckAndMutateRow(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::CheckAndMutateRowRequest const& request,
    OperationContext& operation_context) {
  return UnaryHelper<StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>>(
      pool_, operation_context, [&](BigtableStub& stub) {
        return stub.CheckAndMutateRow(context, options, request,
                                      operation_context);
      });
}

StatusOr<google::bigtable::v2::PingAndWarmResponse>
BigtableRandomTwoLeastUsed::PingAndWarm(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::PingAndWarmRequest const& request,
    OperationContext& operation_context) {
  return UnaryHelper<StatusOr<google::bigtable::v2::PingAndWarmResponse>>(
      pool_, operation_context, [&](BigtableStub& stub) {
        return stub.PingAndWarm(context, options, request, operation_context);
      });
}

StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>
BigtableRandomTwoLeastUsed::ReadModifyWriteRow(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::ReadModifyWriteRowRequest const& request,
    OperationContext& operation_context) {
  return UnaryHelper<
      StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>>(
      pool_, operation_context, [&](BigtableStub& stub) {
        return stub.ReadModifyWriteRow(context, options, request,
                                       operation_context);
      });
}

StatusOr<google::bigtable::v2::PrepareQueryResponse>
BigtableRandomTwoLeastUsed::PrepareQuery(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::PrepareQueryRequest const& request,
    OperationContext& operation_context) {
  return UnaryHelper<StatusOr<google::bigtable::v2::PrepareQueryResponse>>(
      pool_, operation_context, [&](BigtableStub& stub) {
        return stub.PrepareQuery(context, options, request, operation_context);
      });
}

std::unique_ptr<
    internal::StreamingReadRpc<google::bigtable::v2::ExecuteQueryResponse>>
BigtableRandomTwoLeastUsed::ExecuteQuery(
    std::shared_ptr<grpc::ClientContext> context, Options const& options,
    google::bigtable::v2::ExecuteQueryRequest const& request,
    std::shared_ptr<OperationContext> operation_context) {
  return StreamingHelper<google::bigtable::v2::ExecuteQueryResponse>(
      pool_, operation_context,
      [&, context = std::move(context),
       operation_context](BigtableStub& stub) mutable {
        return stub.ExecuteQuery(std::move(context), options, request,
                                 std::move(operation_context));
      });
}

std::unique_ptr<
    internal::AsyncStreamingReadRpc<google::bigtable::v2::ReadRowsResponse>>
BigtableRandomTwoLeastUsed::AsyncReadRows(
    CompletionQueue const& cq, std::shared_ptr<grpc::ClientContext> context,
    internal::ImmutableOptions options,
    google::bigtable::v2::ReadRowsRequest const& request,
    std::shared_ptr<OperationContext> operation_context) {
  return AsyncStreamingHelper<google::bigtable::v2::ReadRowsResponse>(
      pool_, operation_context,
      [&, context = std::move(context), options = std::move(options),
       operation_context](BigtableStub& stub) mutable {
        return stub.AsyncReadRows(cq, std::move(context), std::move(options),
                                  request, std::move(operation_context));
      });
}

std::unique_ptr<internal::AsyncStreamingReadRpc<
    google::bigtable::v2::SampleRowKeysResponse>>
BigtableRandomTwoLeastUsed::AsyncSampleRowKeys(
    CompletionQueue const& cq, std::shared_ptr<grpc::ClientContext> context,
    internal::ImmutableOptions options,
    google::bigtable::v2::SampleRowKeysRequest const& request,
    std::shared_ptr<OperationContext> operation_context) {
  return AsyncStreamingHelper<google::bigtable::v2::SampleRowKeysResponse>(
      pool_, operation_context,
      [&, context = std::move(context), options = std::move(options),
       operation_context](BigtableStub& stub) mutable {
        return stub.AsyncSampleRowKeys(cq, std::move(context),
                                       std::move(options), request,
                                       std::move(operation_context));
      });
}

future<StatusOr<google::bigtable::v2::MutateRowResponse>>
BigtableRandomTwoLeastUsed::AsyncMutateRow(
    CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
    internal::ImmutableOptions options,
    google::bigtable::v2::MutateRowRequest const& request,
    std::shared_ptr<OperationContext> operation_context) {
  return AsyncHelper<future<StatusOr<google::bigtable::v2::MutateRowResponse>>>(
      pool_, operation_context,
      [&, context = std::move(context), options = std::move(options),
       operation_context](BigtableStub& stub) mutable {
        return stub.AsyncMutateRow(cq, std::move(context), std::move(options),
                                   request, std::move(operation_context));
      });
}

std::unique_ptr<
    internal::AsyncStreamingReadRpc<google::bigtable::v2::MutateRowsResponse>>
BigtableRandomTwoLeastUsed::AsyncMutateRows(
    CompletionQueue const& cq, std::shared_ptr<grpc::ClientContext> context,
    internal::ImmutableOptions options,
    google::bigtable::v2::MutateRowsRequest const& request,
    std::shared_ptr<OperationContext> operation_context) {
  return AsyncStreamingHelper<google::bigtable::v2::MutateRowsResponse>(
      pool_, operation_context,
      [&, context = std::move(context), options = std::move(options),
       operation_context](BigtableStub& stub) mutable {
        return stub.AsyncMutateRows(cq, std::move(context), std::move(options),
                                    request, std::move(operation_context));
      });
}

future<StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>>
BigtableRandomTwoLeastUsed::AsyncCheckAndMutateRow(
    CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
    internal::ImmutableOptions options,
    google::bigtable::v2::CheckAndMutateRowRequest const& request,
    std::shared_ptr<OperationContext> operation_context) {
  return AsyncHelper<
      future<StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>>>(
      pool_, operation_context,
      [&, context = std::move(context), options = std::move(options),
       operation_context](BigtableStub& stub) mutable {
        return stub.AsyncCheckAndMutateRow(cq, std::move(context),
                                           std::move(options), request,
                                           std::move(operation_context));
      });
}

future<StatusOr<google::bigtable::v2::PingAndWarmResponse>>
BigtableRandomTwoLeastUsed::AsyncPingAndWarm(
    CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
    internal::ImmutableOptions options,
    google::bigtable::v2::PingAndWarmRequest const& request,
    std::shared_ptr<OperationContext> operation_context) {
  return AsyncHelper<
      future<StatusOr<google::bigtable::v2::PingAndWarmResponse>>>(
      pool_, operation_context,
      [&, context = std::move(context), options = std::move(options),
       operation_context](BigtableStub& stub) mutable {
        return stub.AsyncPingAndWarm(cq, std::move(context), std::move(options),
                                     request, std::move(operation_context));
      });
}

future<StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>>
BigtableRandomTwoLeastUsed::AsyncReadModifyWriteRow(
    CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
    internal::ImmutableOptions options,
    google::bigtable::v2::ReadModifyWriteRowRequest const& request,
    std::shared_ptr<OperationContext> operation_context) {
  return AsyncHelper<
      future<StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>>>(
      pool_, operation_context,
      [&, context = std::move(context), options = std::move(options),
       operation_context](BigtableStub& stub) mutable {
        return stub.AsyncReadModifyWriteRow(cq, std::move(context),
                                            std::move(options), request,
                                            std::move(operation_context));
      });
}

future<StatusOr<google::bigtable::v2::PrepareQueryResponse>>
BigtableRandomTwoLeastUsed::AsyncPrepareQuery(
    CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
    internal::ImmutableOptions options,
    google::bigtable::v2::PrepareQueryRequest const& request,
    std::shared_ptr<OperationContext> operation_context) {
  return AsyncHelper<
      future<StatusOr<google::bigtable::v2::PrepareQueryResponse>>>(
      pool_, operation_context,
      [&, context = std::move(context), options = std::move(options),
       operation_context](BigtableStub& stub) mutable {
        return stub.AsyncPrepareQuery(cq, std::move(context),
                                      std::move(options), request,
                                      std::move(operation_context));
      });
}

StatusOr<google::bigtable::v2::ClientConfiguration>
BigtableRandomTwoLeastUsed::GetClientConfiguration(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::GetClientConfigurationRequest const& request,
    OperationContext& operation_context) {
  return UnaryHelper<StatusOr<google::bigtable::v2::ClientConfiguration>>(
      pool_, operation_context, [&](BigtableStub& stub) {
        return stub.GetClientConfiguration(context, options, request,
                                           operation_context);
      });
}

std::unique_ptr<
    AsyncStreamingReadWriteRpc<google::bigtable::v2::SessionRequest,
                               google::bigtable::v2::SessionResponse>>
BigtableRandomTwoLeastUsed::AsyncOpenTable(
    CompletionQueue const& cq, std::shared_ptr<grpc::ClientContext> context,
    internal::ImmutableOptions options,
    std::shared_ptr<OperationContext> operation_context) {
  return AsyncStreamingHelper<google::bigtable::v2::SessionRequest,
                              google::bigtable::v2::SessionResponse>(
      pool_, operation_context,
      [&, context = std::move(context), options = std::move(options),
       operation_context](BigtableStub& stub) mutable {
        return stub.AsyncOpenTable(cq, std::move(context), std::move(options),
                                   std::move(operation_context));
      });
}

std::unique_ptr<
    AsyncStreamingReadWriteRpc<google::bigtable::v2::SessionRequest,
                               google::bigtable::v2::SessionResponse>>
BigtableRandomTwoLeastUsed::AsyncOpenAuthorizedView(
    CompletionQueue const& cq, std::shared_ptr<grpc::ClientContext> context,
    internal::ImmutableOptions options,
    std::shared_ptr<OperationContext> operation_context) {
  return AsyncStreamingHelper<google::bigtable::v2::SessionRequest,
                              google::bigtable::v2::SessionResponse>(
      pool_, operation_context,
      [&, context = std::move(context), options = std::move(options),
       operation_context](BigtableStub& stub) mutable {
        return stub.AsyncOpenAuthorizedView(cq, std::move(context),
                                            std::move(options),
                                            std::move(operation_context));
      });
}

std::unique_ptr<
    AsyncStreamingReadWriteRpc<google::bigtable::v2::SessionRequest,
                               google::bigtable::v2::SessionResponse>>
BigtableRandomTwoLeastUsed::AsyncOpenMaterializedView(
    CompletionQueue const& cq, std::shared_ptr<grpc::ClientContext> context,
    internal::ImmutableOptions options,
    std::shared_ptr<OperationContext> operation_context) {
  return AsyncStreamingHelper<google::bigtable::v2::SessionRequest,
                              google::bigtable::v2::SessionResponse>(
      pool_, operation_context,
      [&, context = std::move(context), options = std::move(options),
       operation_context](BigtableStub& stub) mutable {
        return stub.AsyncOpenMaterializedView(cq, std::move(context),
                                              std::move(options),
                                              std::move(operation_context));
      });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

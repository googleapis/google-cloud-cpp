// Copyright 2025 Google LLC
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
#include <memory>
#include <mutex>
#include <vector>

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
  absl::optional<Status> Read(T* response) override {
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
  future<absl::optional<T>> Read() override { return child_->Read(); }
  future<Status> Finish() override { return child_->Finish(); }
  RpcMetadata GetRequestMetadata() const override {
    return child_->GetRequestMetadata();
  }

 private:
  std::unique_ptr<google::cloud::internal::AsyncStreamingReadRpc<T>> child_;
  std::function<void(void)> on_destruction_;
};

}  // namespace

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::ReadRowsResponse>>
BigtableRandomTwoLeastUsed::ReadRows(
    std::shared_ptr<grpc::ClientContext> context, Options const& options,
    google::bigtable::v2::ReadRowsRequest const& request) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto child = Child();
  auto stub = child->AcquireStub();
  auto result = stub->ReadRows(std::move(context), options, request);
  auto release_fn = [weak = child->MakeWeak()] {
    auto child = weak.lock();
    if (child) child->ReleaseStub();
  };
  return std::make_unique<
      StreamingReadRpcTracking<google::bigtable::v2::ReadRowsResponse>>(
      std::move(result), std::move(release_fn));
}

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::SampleRowKeysResponse>>
BigtableRandomTwoLeastUsed::SampleRowKeys(
    std::shared_ptr<grpc::ClientContext> context, Options const& options,
    google::bigtable::v2::SampleRowKeysRequest const& request) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto child = Child();
  auto stub = child->AcquireStub();
  auto result = stub->SampleRowKeys(std::move(context), options, request);
  auto release_fn = [weak = child->MakeWeak()] {
    auto child = weak.lock();
    if (child) child->ReleaseStub();
  };
  return std::make_unique<
      StreamingReadRpcTracking<google::bigtable::v2::SampleRowKeysResponse>>(
      std::move(result), std::move(release_fn));
}

StatusOr<google::bigtable::v2::MutateRowResponse>
BigtableRandomTwoLeastUsed::MutateRow(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::MutateRowRequest const& request) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto child = Child();
  auto stub = child->AcquireStub();
  auto result = stub->MutateRow(context, options, request);
  child->ReleaseStub();
  return result;
}

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::MutateRowsResponse>>
BigtableRandomTwoLeastUsed::MutateRows(
    std::shared_ptr<grpc::ClientContext> context, Options const& options,
    google::bigtable::v2::MutateRowsRequest const& request) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto child = Child();
  auto stub = child->AcquireStub();
  auto result = stub->MutateRows(std::move(context), options, request);
  auto release_fn = [weak = child->MakeWeak()] {
    auto child = weak.lock();
    if (child) child->ReleaseStub();
  };
  return std::make_unique<
      StreamingReadRpcTracking<google::bigtable::v2::MutateRowsResponse>>(
      std::move(result), std::move(release_fn));
}

StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>
BigtableRandomTwoLeastUsed::CheckAndMutateRow(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::CheckAndMutateRowRequest const& request) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto child = Child();
  auto stub = child->AcquireStub();
  auto result = stub->CheckAndMutateRow(context, options, request);
  child->ReleaseStub();
  return result;
}

StatusOr<google::bigtable::v2::PingAndWarmResponse>
BigtableRandomTwoLeastUsed::PingAndWarm(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::PingAndWarmRequest const& request) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto child = Child();
  auto stub = child->AcquireStub();
  auto result = stub->PingAndWarm(context, options, request);
  child->ReleaseStub();
  return result;
}

StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>
BigtableRandomTwoLeastUsed::ReadModifyWriteRow(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::ReadModifyWriteRowRequest const& request) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto child = Child();
  auto stub = child->AcquireStub();
  auto result = stub->ReadModifyWriteRow(context, options, request);
  child->ReleaseStub();
  return result;
}

StatusOr<google::bigtable::v2::PrepareQueryResponse>
BigtableRandomTwoLeastUsed::PrepareQuery(
    grpc::ClientContext& context, Options const& options,
    google::bigtable::v2::PrepareQueryRequest const& request) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto child = Child();
  auto stub = child->AcquireStub();
  auto result = stub->PrepareQuery(context, options, request);
  child->ReleaseStub();
  return result;
}

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::ExecuteQueryResponse>>
BigtableRandomTwoLeastUsed::ExecuteQuery(
    std::shared_ptr<grpc::ClientContext> context, Options const& options,
    google::bigtable::v2::ExecuteQueryRequest const& request) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto child = Child();
  auto stub = child->AcquireStub();
  auto result = stub->ExecuteQuery(std::move(context), options, request);
  auto release_fn = [weak = child->MakeWeak()] {
    auto child = weak.lock();
    if (child) child->ReleaseStub();
  };
  return std::make_unique<
      StreamingReadRpcTracking<google::bigtable::v2::ExecuteQueryResponse>>(
      std::move(result), std::move(release_fn));
}

std::unique_ptr<google::cloud::internal::AsyncStreamingReadRpc<
    google::bigtable::v2::ReadRowsResponse>>
BigtableRandomTwoLeastUsed::AsyncReadRows(
    google::cloud::CompletionQueue const& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::bigtable::v2::ReadRowsRequest const& request) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto child = Child();
  auto stub = child->AcquireStub();
  auto result =
      stub->AsyncReadRows(cq, std::move(context), std::move(options), request);
  auto release_fn = [weak = child->MakeWeak()] {
    auto child = weak.lock();
    if (child) child->ReleaseStub();
  };
  return std::make_unique<
      AsyncStreamingReadRpcTracking<google::bigtable::v2::ReadRowsResponse>>(
      std::move(result), std::move(release_fn));
}

std::unique_ptr<google::cloud::internal::AsyncStreamingReadRpc<
    google::bigtable::v2::SampleRowKeysResponse>>
BigtableRandomTwoLeastUsed::AsyncSampleRowKeys(
    google::cloud::CompletionQueue const& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::bigtable::v2::SampleRowKeysRequest const& request) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto child = Child();
  auto stub = child->AcquireStub();
  auto result = stub->AsyncSampleRowKeys(cq, std::move(context),
                                         std::move(options), request);
  auto release_fn = [weak = child->MakeWeak()] {
    auto child = weak.lock();
    if (child) child->ReleaseStub();
  };
  return std::make_unique<AsyncStreamingReadRpcTracking<
      google::bigtable::v2::SampleRowKeysResponse>>(std::move(result),
                                                    std::move(release_fn));
}

future<StatusOr<google::bigtable::v2::MutateRowResponse>>
BigtableRandomTwoLeastUsed::AsyncMutateRow(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::bigtable::v2::MutateRowRequest const& request) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto child = Child();
  auto stub = child->AcquireStub();
  auto result =
      stub->AsyncMutateRow(cq, std::move(context), std::move(options), request);
  child->ReleaseStub();
  return result;
}

std::unique_ptr<google::cloud::internal::AsyncStreamingReadRpc<
    google::bigtable::v2::MutateRowsResponse>>
BigtableRandomTwoLeastUsed::AsyncMutateRows(
    google::cloud::CompletionQueue const& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::bigtable::v2::MutateRowsRequest const& request) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto child = Child();
  auto stub = child->AcquireStub();
  auto result = stub->AsyncMutateRows(cq, std::move(context),
                                      std::move(options), request);
  auto release_fn = [weak = child->MakeWeak()] {
    auto child = weak.lock();
    if (child) child->ReleaseStub();
  };

  return std::make_unique<
      AsyncStreamingReadRpcTracking<google::bigtable::v2::MutateRowsResponse>>(
      std::move(result), std::move(release_fn));
}

future<StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>>
BigtableRandomTwoLeastUsed::AsyncCheckAndMutateRow(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::bigtable::v2::CheckAndMutateRowRequest const& request) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto child = Child();
  auto stub = child->AcquireStub();
  auto result = stub->AsyncCheckAndMutateRow(cq, std::move(context),
                                             std::move(options), request);
  child->ReleaseStub();
  return result;
}

future<StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>>
BigtableRandomTwoLeastUsed::AsyncReadModifyWriteRow(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::bigtable::v2::ReadModifyWriteRowRequest const& request) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto child = Child();
  auto stub = child->AcquireStub();
  auto result = stub->AsyncReadModifyWriteRow(cq, std::move(context),
                                              std::move(options), request);
  child->ReleaseStub();
  return result;
}

future<StatusOr<google::bigtable::v2::PrepareQueryResponse>>
BigtableRandomTwoLeastUsed::AsyncPrepareQuery(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::bigtable::v2::PrepareQueryRequest const& request) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto child = Child();
  auto stub = child->AcquireStub();
  auto result = stub->AsyncPrepareQuery(cq, std::move(context),
                                        std::move(options), request);
  child->ReleaseStub();
  return result;
}

std::shared_ptr<ChannelUsageWrapper<BigtableStub>>
BigtableRandomTwoLeastUsed::Child() {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  return pool_->GetChannelRandomTwoLeastUsed();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

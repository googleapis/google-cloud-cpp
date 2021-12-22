// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/storage_stub.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/getenv.h"
#include "absl/memory/memory.h"
#include <google/storage/v2/storage.grpc.pb.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

class StorageStubImpl : public StorageStub {
 public:
  explicit StorageStubImpl(
      std::unique_ptr<google::storage::v2::Storage::StubInterface> impl)
      : impl_(std::move(impl)) {}
  ~StorageStubImpl() override = default;

  std::unique_ptr<ReadObjectStream> ReadObject(
      std::unique_ptr<grpc::ClientContext> context,
      google::storage::v2::ReadObjectRequest const& request) override {
    using ::google::cloud::internal::StreamingReadRpcImpl;
    auto stream = impl_->ReadObject(context.get(), request);
    return absl::make_unique<
        StreamingReadRpcImpl<google::storage::v2::ReadObjectResponse>>(
        std::move(context), std::move(stream));
  }

  std::unique_ptr<WriteObjectStream> WriteObject(
      std::unique_ptr<grpc::ClientContext> context) override {
    using ::google::cloud::internal::StreamingWriteRpcImpl;
    using ResponseType = ::google::storage::v2::WriteObjectResponse;
    using RequestType = ::google::storage::v2::WriteObjectRequest;
    auto response = absl::make_unique<ResponseType>();
    auto stream = impl_->WriteObject(context.get(), response.get());
    return absl::make_unique<StreamingWriteRpcImpl<RequestType, ResponseType>>(
        std::move(context), std::move(response), std::move(stream));
  }

  StatusOr<google::storage::v2::StartResumableWriteResponse>
  StartResumableWrite(
      grpc::ClientContext& context,
      google::storage::v2::StartResumableWriteRequest const& request) override {
    google::storage::v2::StartResumableWriteResponse response;
    auto status = impl_->StartResumableWrite(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v2::QueryWriteStatusResponse> QueryWriteStatus(
      grpc::ClientContext& context,
      google::storage::v2::QueryWriteStatusRequest const& request) override {
    google::storage::v2::QueryWriteStatusResponse response;
    auto status = impl_->QueryWriteStatus(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

 private:
  std::unique_ptr<google::storage::v2::Storage::StubInterface> impl_;
};

}  // namespace

std::shared_ptr<StorageStub> MakeDefaultStorageStub(
    std::shared_ptr<grpc::Channel> channel) {
  return std::make_shared<StorageStubImpl>(
      google::storage::v2::Storage::NewStub(std::move(channel)));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

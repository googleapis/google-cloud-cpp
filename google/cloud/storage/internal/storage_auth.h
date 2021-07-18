// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_STORAGE_AUTH_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_STORAGE_AUTH_H

#include "google/cloud/storage/internal/storage_stub.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/unified_grpc_credentials.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

class StorageAuth : public StorageStub {
 public:
  explicit StorageAuth(
      std::shared_ptr<google::cloud::internal::GrpcAuthenticationStrategy> auth,
      std::shared_ptr<StorageStub> child)
      : auth_(std::move(auth)), child_(std::move(child)) {}
  ~StorageAuth() override = default;

  std::unique_ptr<ObjectMediaStream> GetObjectMedia(
      std::unique_ptr<grpc::ClientContext> context,
      google::storage::v1::GetObjectMediaRequest const& request) override;

  std::unique_ptr<InsertStream> InsertObjectMedia(
      std::unique_ptr<grpc::ClientContext> context) override;

  StatusOr<google::storage::v1::StartResumableWriteResponse>
  StartResumableWrite(
      grpc::ClientContext& context,
      google::storage::v1::StartResumableWriteRequest const& request) override;
  StatusOr<google::storage::v1::QueryWriteStatusResponse> QueryWriteStatus(
      grpc::ClientContext& context,
      google::storage::v1::QueryWriteStatusRequest const& request) override;

 private:
  std::shared_ptr<google::cloud::internal::GrpcAuthenticationStrategy> auth_;
  std::shared_ptr<StorageStub> child_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_STORAGE_AUTH_H

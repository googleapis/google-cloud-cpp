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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_STORAGE_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_STORAGE_STUB_H

#include "google/cloud/storage/version.h"
#include "google/cloud/internal/streaming_read_rpc.h"
#include "google/cloud/internal/streaming_write_rpc.h"
#include "google/cloud/status_or.h"
#include <google/storage/v1/storage.pb.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

class StorageStub {
 public:
  virtual ~StorageStub() = default;

  using ObjectMediaStream = ::google::cloud::internal::StreamingReadRpc<
      google::storage::v1::GetObjectMediaResponse>;
  virtual std::unique_ptr<ObjectMediaStream> GetObjectMedia(
      std::unique_ptr<grpc::ClientContext> context,
      google::storage::v1::GetObjectMediaRequest const& request) = 0;

  using InsertStream = ::google::cloud::internal::StreamingWriteRpc<
      google::storage::v1::InsertObjectRequest, google::storage::v1::Object>;
  virtual std::unique_ptr<InsertStream> InsertObjectMedia(
      std::unique_ptr<grpc::ClientContext> context) = 0;

  virtual StatusOr<google::storage::v1::StartResumableWriteResponse>
  StartResumableWrite(
      grpc::ClientContext& context,
      google::storage::v1::StartResumableWriteRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::QueryWriteStatusResponse>
  QueryWriteStatus(
      grpc::ClientContext& context,
      google::storage::v1::QueryWriteStatusRequest const& request) = 0;
};

std::shared_ptr<StorageStub> MakeDefaultStorageStub(
    std::shared_ptr<grpc::Channel> channel);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_STORAGE_STUB_H

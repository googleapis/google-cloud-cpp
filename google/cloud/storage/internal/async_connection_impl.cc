// Copyright 2022 Google LLC
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

#include "google/cloud/storage/internal/async_connection_impl.h"
#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/internal/grpc_configure_client_context.h"
#include "google/cloud/storage/internal/grpc_object_request_parser.h"
#include "google/cloud/storage/internal/storage_stub_factory.h"
#include "google/cloud/internal/async_retry_loop.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

AsyncConnectionImpl::AsyncConnectionImpl(CompletionQueue cq,
                                         std::shared_ptr<StorageStub> stub,
                                         Options options)
    : cq_(std::move(cq)),
      stub_(std::move(stub)),
      options_(std::move(options)) {}

future<Status> AsyncConnectionImpl::AsyncDeleteObject(
    storage::internal::DeleteObjectRequest const& request) {
  auto proto = storage::internal::GrpcObjectRequestParser::ToProto(request);
  auto const idempotency = idempotency_policy()->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  auto& stub = stub_;
  return google::cloud::internal::AsyncRetryLoop(
      retry_policy(), backoff_policy(), idempotency, cq_,
      [stub, request](CompletionQueue& cq,
                      std::unique_ptr<grpc::ClientContext> context,
                      google::storage::v2::DeleteObjectRequest const& proto) {
        ApplyQueryParameters(*context, request);
        return stub->AsyncDeleteObject(cq, std::move(context), proto);
      },
      proto, __func__);
}

std::shared_ptr<AsyncConnection> MakeAsyncConnection(CompletionQueue cq,
                                                     Options options) {
  options = storage::internal::DefaultOptionsGrpc(std::move(options));
  auto stub = CreateStorageStub(cq, options);
  return MakeAsyncConnection(std::move(cq), std::move(stub),
                             std::move(options));
}

std::shared_ptr<AsyncConnection> MakeAsyncConnection(
    CompletionQueue cq, std::shared_ptr<StorageStub> stub, Options options) {
  return std::make_shared<AsyncConnectionImpl>(std::move(cq), std::move(stub),
                                               std::move(options));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

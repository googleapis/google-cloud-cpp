// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/async/open_object.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/make_status.h"
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string RequestParams(
    google::storage::v2::BidiReadObjectRequest const& request) {
  auto const& read_spec = request.read_object_spec();
  if (read_spec.has_routing_token()) {
    return absl::StrCat("bucket=", read_spec.bucket(),
                        "&routing_token=", read_spec.routing_token());
  }
  return absl::StrCat("bucket=", read_spec.bucket());
}

OpenObject::OpenObject(storage_internal::StorageStub& stub, CompletionQueue& cq,
                       std::shared_ptr<grpc::ClientContext> context,
                       google::cloud::internal::ImmutableOptions options,
                       google::storage::v2::BidiReadObjectRequest request)
    : rpc_(std::make_shared<OpenStream>(CreateRpc(
          stub, cq, std::move(context), std::move(options), request))),
      initial_request_(std::move(request)) {}

future<StatusOr<OpenStreamResult>> OpenObject::Call() {
  auto future = promise_.get_future();
  rpc_->Start().then([w = WeakFromThis()](auto f) {
    if (auto self = w.lock()) self->OnStart(f.get());
  });
  return future;
}

std::weak_ptr<OpenObject> OpenObject::WeakFromThis() {
  return shared_from_this();
}

std::unique_ptr<OpenStream::StreamingRpc> OpenObject::CreateRpc(
    storage_internal::StorageStub& stub, CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::storage::v2::BidiReadObjectRequest const& request) {
  auto p = RequestParams(request);
  if (!p.empty()) context->AddMetadata("x-goog-request-params", std::move(p));
  return stub.AsyncBidiReadObject(cq, std::move(context), std::move(options));
}

void OpenObject::OnStart(bool ok) {
  if (!ok) return DoFinish();
  rpc_->Write(initial_request_).then([w = WeakFromThis()](auto f) {
    if (auto self = w.lock()) self->OnWrite(f.get());
  });
}

void OpenObject::OnWrite(bool ok) {
  if (!ok) return DoFinish();
  rpc_->Read().then([w = WeakFromThis()](auto f) {
    if (auto self = w.lock()) self->OnRead(f.get());
  });
}

void OpenObject::OnRead(
    absl::optional<google::storage::v2::BidiReadObjectResponse> response) {
  if (!response) return DoFinish();
  promise_.set_value(OpenStreamResult{std::move(rpc_), std::move(*response)});
}

void OpenObject::DoFinish() {
  rpc_->Finish().then([w = WeakFromThis()](auto f) {
    if (auto self = w.lock()) self->OnFinish(f.get());
  });
}

void OpenObject::OnFinish(Status status) {
  if (!status.ok()) return promise_.set_value(std::move(status));
  // This should not happen, it indicates an EOF on the stream, but we
  // did not ask to close it.
  promise_.set_value(google::cloud::internal::InternalError(
      "could not open stream, but the stream closed successfully",
      GCP_ERROR_INFO()));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

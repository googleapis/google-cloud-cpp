// Copyright 2023 Google LLC
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

#include "google/cloud/storage/internal/async/writer_connection_impl.h"
#include "google/cloud/storage/internal/async/partial_upload.h"
#include "google/cloud/storage/internal/async/write_payload_impl.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/internal/grpc/object_metadata_parser.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

auto HandleFinishAfterError(std::string msg) {
  return [m = std::move(msg)](future<Status> f) {
    auto status = f.get();
    if (!status.ok()) return status;
    // A `Write()` or `Read()` operation unexpectedly returned `ok == false`.
    // The `Finish()` call should return the detailed error, but returned
    // "success". This is some kind of internal error in the client library,
    // gRPC, or the service.
    return internal::InternalError(std::move(m), GCP_ERROR_INFO());
  };
}

auto HandleFinishAfterError(Status s) {
  return [s = std::move(s)](future<Status>) mutable { return std::move(s); };
}

}  // namespace

AsyncWriterConnectionImpl::AsyncWriterConnectionImpl(
    google::cloud::internal::ImmutableOptions options,
    std::unique_ptr<StreamingRpc> impl, std::string upload_id,
    std::shared_ptr<storage::internal::HashFunction> hash_function,
    std::int64_t persisted_size)
    : options_(std::move(options)),
      impl_(std::move(impl)),
      upload_id_(std::move(upload_id)),
      hash_function_(std::move(hash_function)),
      persisted_state_(persisted_size),
      offset_(persisted_size),
      finished_(on_finish_.get_future()) {}

AsyncWriterConnectionImpl::AsyncWriterConnectionImpl(
    google::cloud::internal::ImmutableOptions options,
    std::unique_ptr<StreamingRpc> impl, std::string upload_id,
    std::shared_ptr<storage::internal::HashFunction> hash_function,
    storage::ObjectMetadata metadata)
    : options_(std::move(options)),
      impl_(std::move(impl)),
      upload_id_(std::move(upload_id)),
      hash_function_(std::move(hash_function)),
      persisted_state_(std::move(metadata)),
      finished_(on_finish_.get_future()) {}

AsyncWriterConnectionImpl::~AsyncWriterConnectionImpl() {
  // Cancel the streaming RPC.
  impl_->Cancel();
  // It is safe to call Finish() because:
  // (1) this is a no-op if it was already called.
  // (2) calls to `Write()`, `Finalize()`, and `Query()` must have completed
  //     by the time the destructor is called
  Finish();
  // When `impl_->Finish()` is satisfied then `finished_` is satisfied too.
  // This extends the lifetime of `impl_` until it is safe to delete.
  finished_.then([impl = std::move(impl_)](auto) mutable {
    // Break the ownership cycle between the completion queue and this callback.
    impl.reset();
  });
}

std::string AsyncWriterConnectionImpl::UploadId() const { return upload_id_; }

absl::variant<std::int64_t, storage::ObjectMetadata>
AsyncWriterConnectionImpl::PersistedState() const {
  return persisted_state_;
}

future<Status> AsyncWriterConnectionImpl::Write(
    storage_experimental::WritePayload payload) {
  auto write = MakeRequest();
  auto p = WritePayloadImpl::GetImpl(payload);
  auto size = p.size();
  auto coro = PartialUpload::Call(impl_, hash_function_, std::move(write),
                                  std::move(p), PartialUpload::kNone);

  return coro->Start().then([coro, size, this](auto f) mutable {
    coro.reset();  // breaks the cycle between the completion queue and coro
    return OnPartialUpload(size, f.get());
  });
}

future<StatusOr<storage::ObjectMetadata>> AsyncWriterConnectionImpl::Finalize(
    storage_experimental::WritePayload payload) {
  auto write = MakeRequest();
  write.set_finish_write(true);

  auto p = WritePayloadImpl::GetImpl(payload);
  auto size = p.size();
  auto coro = PartialUpload::Call(impl_, hash_function_, std::move(write),
                                  std::move(p), PartialUpload::kFinalize);
  return coro->Start().then([coro, size, this](auto f) mutable {
    coro.reset();  // breaks the cycle between the completion queue and coro
    return OnFinalUpload(size, f.get());
  });
}

future<Status> AsyncWriterConnectionImpl::Flush(
    storage_experimental::WritePayload payload) {
  auto write = MakeRequest();
  auto p = WritePayloadImpl::GetImpl(payload);
  auto size = p.size();
  auto coro = PartialUpload::Call(impl_, hash_function_, std::move(write),
                                  std::move(p), PartialUpload::kFlush);

  return coro->Start().then([coro, size, this](auto f) mutable {
    coro.reset();  // breaks the cycle between the completion queue and coro
    return OnPartialUpload(size, f.get());
  });
}

future<StatusOr<std::int64_t>> AsyncWriterConnectionImpl::Query() {
  return impl_->Read().then([this](auto f) { return OnQuery(f.get()); });
}

RpcMetadata AsyncWriterConnectionImpl::GetRequestMetadata() {
  return impl_->GetRequestMetadata();
}

google::storage::v2::BidiWriteObjectRequest
AsyncWriterConnectionImpl::MakeRequest() {
  auto request = google::storage::v2::BidiWriteObjectRequest{};
  if (first_request_) {
    request.set_upload_id(upload_id_);
    first_request_ = false;
  }
  request.set_write_offset(offset_);
  return request;
}

future<Status> AsyncWriterConnectionImpl::OnPartialUpload(
    std::size_t upload_size, StatusOr<bool> success) {
  if (!success) {
    return Finish().then(HandleFinishAfterError(std::move(success).status()));
  }
  if (!*success) {
    return Finish().then(
        HandleFinishAfterError("Expected Finish() error after non-ok Write()"));
  }
  offset_ += upload_size;
  return make_ready_future(Status{});
}

future<StatusOr<storage::ObjectMetadata>>
AsyncWriterConnectionImpl::OnFinalUpload(std::size_t upload_size,
                                         StatusOr<bool> success) {
  auto transform = [](future<Status> f) {
    return StatusOr<storage::ObjectMetadata>(f.get());
  };
  if (!success) {
    return Finish()
        .then(HandleFinishAfterError(std::move(success).status()))
        .then(transform);
  }
  if (!*success) {
    return Finish()
        .then(HandleFinishAfterError(
            "Expected error in Finish() after non-ok Write()"))
        .then(transform);
  }
  offset_ += upload_size;
  return impl_->Read()
      .then([this](auto f) { return OnQuery(f.get()); })
      .then([this](auto g) -> StatusOr<storage::ObjectMetadata> {
        auto status = g.get();
        if (!status) return std::move(status).status();
        if (!absl::holds_alternative<storage::ObjectMetadata>(
                persisted_state_)) {
          return internal::InternalError(
              "no object metadata returned after finalizing upload",
              GCP_ERROR_INFO());
        }
        return absl::get<storage::ObjectMetadata>(persisted_state_);
      });
}

future<StatusOr<std::int64_t>> AsyncWriterConnectionImpl::OnQuery(
    absl::optional<google::storage::v2::BidiWriteObjectResponse> response) {
  if (!response.has_value()) {
    return Finish()
        .then(HandleFinishAfterError(
            "Expected error in Finish() after non-ok Read()"))
        .then([](auto f) { return StatusOr<std::int64_t>(f.get()); });
  }
  if (response->has_persisted_size()) {
    persisted_state_ = response->persisted_size();
    return make_ready_future(make_status_or(response->persisted_size()));
  }
  if (response->has_resource()) {
    persisted_state_ = FromProto(response->resource(), *options_);
    return make_ready_future(make_status_or(response->resource().size()));
  }
  return make_ready_future(make_status_or(static_cast<std::int64_t>(0)));
}

future<Status> AsyncWriterConnectionImpl::Finish() {
  if (std::exchange(finish_called_, true)) {
    return make_ready_future(
        internal::CancelledError("already finished", GCP_ERROR_INFO()));
  }
  return impl_->Finish().then([p = std::move(on_finish_)](auto f) mutable {
    p.set_value();
    return f.get();
  });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

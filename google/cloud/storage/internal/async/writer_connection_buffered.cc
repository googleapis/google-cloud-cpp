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

#include "google/cloud/storage/internal/async/writer_connection_buffered.h"
#include "google/cloud/storage/internal/async/write_payload_impl.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "absl/strings/cord.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

Status MakeRewindError(absl::string_view upload_id, std::int64_t resend_offset,
                       std::int64_t persisted_size,
                       internal::ErrorInfoBuilder eib) {
  auto const previous = std::to_string(resend_offset);
  auto const returned = std::to_string(persisted_size);
  return internal::InternalError(
      "server persisted_size rewind. This indicates a bug in the client library"
      " or the service.",
      std::move(eib)
          .WithMetadata("gcloud-cpp.storage.upload_id", upload_id)
          .WithMetadata("gcloud-cpp.storage.resend_offset", previous)
          .WithMetadata("gcloud-cpp.storage.persisted_size", returned));
}

Status MakeFastForwardError(absl::string_view upload_id,
                            std::int64_t resend_offset,
                            std::int64_t persisted_size,
                            internal::ErrorInfoBuilder eib) {
  auto const previous = std::to_string(resend_offset);
  auto const returned = std::to_string(persisted_size);
  return internal::InternalError(
      "server persisted_size too high. This can be caused by concurrent"
      " uploads using the same upload id. Most likely an application bug.",
      std::move(eib)
          .WithMetadata("gcloud-cpp.storage.upload_id", upload_id)
          .WithMetadata("gcloud-cpp.storage.resend_offset", previous)
          .WithMetadata("gcloud-cpp.storage.persisted_size", returned));
}

class AsyncWriterConnectionBufferedState
    : public std::enable_shared_from_this<AsyncWriterConnectionBufferedState> {
 public:
  AsyncWriterConnectionBufferedState(
      WriterConnectionFactory factory,
      std::unique_ptr<storage_experimental::AsyncWriterConnection> impl,
      std::size_t buffer_size_lwm, std::size_t buffer_size_hwm)
      : factory_(std::move(factory)),
        buffer_size_lwm_(buffer_size_lwm),
        buffer_size_hwm_(buffer_size_hwm),
        impl_(std::move(impl)) {
    finalized_future_ = finalized_.get_future();
    auto state = impl_->PersistedState();
    if (absl::holds_alternative<google::storage::v2::Object>(state)) {
      SetFinalized(std::unique_lock<std::mutex>(mu_),
                   absl::get<google::storage::v2::Object>(std::move(state)));
      cancelled_ = true;
      resume_status_ = internal::CancelledError("upload already finalized",
                                                GCP_ERROR_INFO());
      return;
    }
    buffer_offset_ = absl::get<std::int64_t>(state);
  }

  void Cancel() {
    std::unique_lock<std::mutex> lk(mu_);
    cancelled_ = true;
    auto impl = Impl(lk);
    lk.unlock();
    return impl->Cancel();
  }

  std::string UploadId() const {
    return UploadId(std::unique_lock<std::mutex>(mu_));
  }

  absl::variant<std::int64_t, google::storage::v2::Object> PersistedState()
      const {
    return Impl(std::unique_lock<std::mutex>(mu_))->PersistedState();
  }

  future<Status> Write(storage_experimental::WritePayload const& p) {
    std::unique_lock<std::mutex> lk(mu_);
    resend_buffer_.Append(WritePayloadImpl::GetImpl(p));
    return HandleNewData(std::move(lk));
  }

  future<StatusOr<google::storage::v2::Object>> Finalize(
      storage_experimental::WritePayload const& p) {
    std::unique_lock<std::mutex> lk(mu_);
    resend_buffer_.Append(WritePayloadImpl::GetImpl(p));
    finalize_ = true;
    HandleNewData(std::move(lk));
    return std::move(finalized_future_);
  }

  future<Status> Flush(storage_experimental::WritePayload const& p) {
    return Write(p);
  }

  future<StatusOr<std::int64_t>> Query() {
    return Impl(std::unique_lock<std::mutex>(mu_))->Query();
  }

  RpcMetadata GetRequestMetadata() {
    return Impl(std::unique_lock<std::mutex>(mu_))->GetRequestMetadata();
  }

 private:
  std::weak_ptr<AsyncWriterConnectionBufferedState> WeakFromThis() {
    return shared_from_this();
  }

  // Cannot use `std::function<>` because we need to capture a move-only
  // `promise<Status>`. Cannot use `absl::AnyInvocable<>` because we need to
  // support versions of Abseil that lack such a class.
  struct BufferShrinkHandler {
    virtual ~BufferShrinkHandler() = default;
    virtual void Execute(Status status) = 0;
  };

  static std::unique_ptr<BufferShrinkHandler> MakeLwmWaiter(promise<Status> p) {
    struct Impl : public BufferShrinkHandler {
      promise<Status> p;

      explicit Impl(promise<Status> pr) : p(std::move(pr)) {}
      ~Impl() override = default;
      void Execute(Status status) override { p.set_value(std::move(status)); }
    };
    return std::make_unique<Impl>(std::move(p));
  }

  future<Status> HandleNewData(std::unique_lock<std::mutex> lk) {
    if (!resume_status_.ok()) return make_ready_future(resume_status_);
    auto const buffer_size = resend_buffer_.size();
    flush_ = buffer_size >= buffer_size_lwm_;
    auto result = make_ready_future(Status{});
    if (buffer_size >= buffer_size_hwm_) {
      auto p = promise<Status>();
      result = p.get_future();
      flush_handlers_.push_back(MakeLwmWaiter(std::move(p)));
    }
    (void)StartWriting(std::move(lk));
    return result;
  }

  void StartWriting(std::unique_lock<std::mutex> lk) {
    if (writing_) return;
    WriteLoop(std::move(lk));
  }

  void WriteLoop(std::unique_lock<std::mutex> lk) {
    writing_ = write_offset_ < resend_buffer_.size();
    if (!writing_ && !finalize_) return;
    auto const n = resend_buffer_.size() - write_offset_;
    auto payload = resend_buffer_.Subcord(write_offset_, n);
    if (finalize_) return FinalizeStep(std::move(lk), std::move(payload));
    if (flush_) return FlushStep(std::move(lk), std::move(payload));
    WriteStep(std::move(lk), std::move(payload));
  }

  void FinalizeStep(std::unique_lock<std::mutex> lk, absl::Cord payload) {
    auto impl = Impl(lk);
    lk.unlock();
    (void)impl->Finalize(WritePayloadImpl::Make(std::move(payload)))
        .then([w = WeakFromThis()](auto f) {
          if (auto self = w.lock()) return self->OnFinalize(f.get());
        });
  }

  void OnFinalize(StatusOr<google::storage::v2::Object> result) {
    if (!result) return Resume(std::move(result).status());
    SetFinalized(std::unique_lock<std::mutex>(mu_), std::move(result));
  }

  void FlushStep(std::unique_lock<std::mutex> lk, absl::Cord payload) {
    auto impl = Impl(lk);
    lk.unlock();
    auto const size = payload.size();
    (void)impl->Flush(WritePayloadImpl::Make(std::move(payload)))
        .then([size, w = WeakFromThis()](auto f) {
          if (auto self = w.lock()) return self->OnFlush(f.get(), size);
        });
  }

  void OnFlush(Status result, std::size_t write_size) {
    if (!result.ok()) return Resume(std::move(result));
    std::unique_lock<std::mutex> lk(mu_);
    write_offset_ += write_size;
    auto impl = Impl(lk);
    lk.unlock();
    impl->Query().then([w = WeakFromThis()](auto f) {
      if (auto self = w.lock()) return self->OnQuery(f.get());
    });
  }

  void OnQuery(StatusOr<std::int64_t> persisted_size) {
    if (!persisted_size) return Resume(std::move(persisted_size).status());
    return OnQuery(std::unique_lock<std::mutex>(mu_), *persisted_size);
  }

  auto ClearHandlers(std::unique_lock<std::mutex> const& /* lk */) {
    decltype(flush_handlers_) tmp;
    flush_handlers_.swap(tmp);
    return tmp;
  }

  auto ClearHandlersIfEmpty(std::unique_lock<std::mutex> const& /* lk */) {
    decltype(flush_handlers_) tmp;
    if (resend_buffer_.size() >= buffer_size_lwm_) return tmp;
    flush_handlers_.swap(tmp);
    return tmp;
  }

  void OnQuery(std::unique_lock<std::mutex> lk, std::int64_t persisted_size) {
    if (persisted_size < buffer_offset_) {
      auto id = UploadId(lk);
      return SetError(std::move(lk),
                      MakeRewindError(std::move(id), buffer_offset_,
                                      persisted_size, GCP_ERROR_INFO()));
    }
    auto const n = persisted_size - buffer_offset_;
    if (n > static_cast<std::int64_t>(resend_buffer_.size())) {
      auto id = UploadId(lk);
      return SetError(std::move(lk),
                      MakeFastForwardError(std::move(id), buffer_offset_,
                                           persisted_size, GCP_ERROR_INFO()));
    }
    resend_buffer_.RemovePrefix(static_cast<std::size_t>(n));
    buffer_offset_ = persisted_size;
    write_offset_ -= static_cast<std::size_t>(n);
    // If the buffer is small enough, collect all the handlers to notify them.
    auto const handlers = ClearHandlersIfEmpty(lk);
    WriteLoop(std::move(lk));
    // The notifications are deferred until the lock is released, as they might
    // call back and try to acquire the lock.
    for (auto const& h : handlers) h->Execute(Status{});
  }

  void WriteStep(std::unique_lock<std::mutex> lk, absl::Cord payload) {
    auto impl = Impl(lk);
    lk.unlock();
    auto const size = payload.size();
    (void)impl->Write(WritePayloadImpl::Make(std::move(payload)))
        .then([size, w = WeakFromThis()](auto f) {
          if (auto self = w.lock()) return self->OnWrite(f.get(), size);
        });
  }

  void OnWrite(Status result, std::size_t write_size) {
    if (!result.ok()) return Resume(std::move(result));
    std::unique_lock<std::mutex> lk(mu_);
    write_offset_ += write_size;
    return WriteLoop(std::move(lk));
  }

  void Resume(Status s) {
    if (!s.ok() && cancelled_) {
      return SetError(std::unique_lock<std::mutex>(mu_), std::move(s));
    }
    factory_().then([w = WeakFromThis()](auto f) {
      if (auto self = w.lock()) return self->OnResume(f.get());
    });
  }

  void OnResume(
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>
          impl) {
    std::unique_lock<std::mutex> lk(mu_);
    if (!impl) return SetError(std::move(lk), std::move(impl).status());
    impl_ = *std::move(impl);
    auto state = impl_->PersistedState();
    if (absl::holds_alternative<google::storage::v2::Object>(state)) {
      return SetFinalized(std::move(lk), absl::get<google::storage::v2::Object>(
                                             std::move(state)));
    }
    OnQuery(std::move(lk), absl::get<std::int64_t>(state));
  }

  void SetFinalized(std::unique_lock<std::mutex> lk,
                    StatusOr<google::storage::v2::Object> r) {
    if (!r) return SetError(std::move(lk), std::move(r).status());
    SetFinalized(std::move(lk), *std::move(r));
  }

  void SetFinalized(std::unique_lock<std::mutex> lk,
                    google::storage::v2::Object object) {
    resend_buffer_.Clear();
    writing_ = false;
    finalize_ = false;
    flush_ = false;
    auto handlers = ClearHandlers(lk);
    promise<StatusOr<google::storage::v2::Object>> finalized{null_promise_t{}};
    finalized.swap(finalized_);
    lk.unlock();
    for (auto& h : handlers) h->Execute(Status{});
    finalized.set_value(std::move(object));
  }

  void SetError(std::unique_lock<std::mutex> lk, Status status) {
    resume_status_ = status;
    writing_ = false;
    finalize_ = false;
    flush_ = false;
    auto handlers = ClearHandlers(lk);
    promise<StatusOr<google::storage::v2::Object>> finalized{null_promise_t{}};
    finalized.swap(finalized_);
    lk.unlock();
    for (auto& h : handlers) h->Execute(status);
    finalized.set_value(std::move(status));
  }

  std::shared_ptr<storage_experimental::AsyncWriterConnection> Impl(
      std::unique_lock<std::mutex> const& /*lk*/) const {
    return impl_;
  }

  std::string UploadId(std::unique_lock<std::mutex> const& lk) const {
    return Impl(lk)->UploadId();
  }

  // Creates new `impl_` instances when needed.
  WriterConnectionFactory const factory_;

  // Request a server-side flush if the buffer goes over this threshold.
  std::size_t const buffer_size_lwm_;

  // Stop sending data if the buffer goes over this threshold. Only
  // start sending data again if the size goes below buffer_size_lwm_.
  std::size_t const buffer_size_hwm_;

  // The remaining member variables need a mutex for access. The background
  // threads may change them as the resend_buffer_ is drained and/or as the
  // reconnect loop resets `impl_`.
  // It may be possible to reduce locking overhead as only one background thread
  // operates on these member variables at a time. That seems like too small an
  // optimization to increase the complexity of the code.
  std::mutex mutable mu_;

  // The state of the resume loop. Once the resume loop fails no more resume
  // or write attempts are made.
  Status resume_status_;

  // The current writer.
  std::shared_ptr<storage_experimental::AsyncWriterConnection> impl_;

  // The result of calling `Finalize()`. Note that only one such call is ever
  // made.
  promise<StatusOr<google::storage::v2::Object>> finalized_;

  // Retrieve the future in the constructor, as some operations reset
  // finalized_.
  future<StatusOr<google::storage::v2::Object>> finalized_future_;

  // The resend buffer. If there is an error, this will have all the data since
  // the last persisted byte and will be resent.
  //
  // If this is larger than `buffer_size_hwm_` then `Write()`, and `Flush()`
  // will return futures that become satisfied only once the buffer size gets
  // below `buffer_size_lwm_`.
  //
  // Note that `Finalize()` does not block when the buffer gets too large. It
  // always blocks on `finalized_`.
  absl::Cord resend_buffer_;

  // If true, all the data to finalize an upload is in `resend_buffer_`.
  bool finalize_ = false;

  // If true, all data should be uploaded with `Flush()`.
  bool flush_ = true;

  // The offset for the first byte in the resend_buffer_.
  std::int64_t buffer_offset_ = 0;

  // The offset in `resend_buffer_` for the last `impl_->Write()` call.
  std::size_t write_offset_ = 0;

  // Handle buffer flush events. Some member functions want to be notified of
  // permanent errors in the resume loop and changes in the buffer size.
  // The most common cases included:
  // - A Write() call that returns an unsatisfied future until the buffer size
  //   is small enough.
  // - A Flush() call that returns an unsatisified future until the buffer is
  //   small enough.
  std::vector<std::unique_ptr<BufferShrinkHandler>> flush_handlers_;

  // True if the writing loop is activate.
  bool writing_ = false;

  // True if cancelled, in which case any RPC failures are final.
  bool cancelled_ = false;
};

/**
 * Implements an `AsyncWriterConnection` that automatically resumes and resends
 * data.
 *
 * This class is used in the implementation of
 * `AsyncClient::StartBufferedUpload()`. Please see that function for the
 * motivation.
 *
 * This implementation of `AsyncWriterConnection` keeps an in-memory
 * `resend_buffer_` of type `absl::Cord`. New data is added to the end of the
 * Cord. Flushed data is removed from the front of the Cord.
 *
 * Applications threads add data by calling `Write()` and `Finalize()`.
 *
 * The buffer is drained by an asynchronous loop running in background threads.
 * This loop starts (if needed) when new data is appended to the
 * `resend_buffer_`. If the buffer is neither full nor approaching fullness
 * the loop calls `impl_->Write()` to upload data to the service.
 *
 * When the application finalizes an upload the loop calls `impl_->Finalize()`
 * and sends any previously buffered data as well as the new data.
 *
 * If the buffer is getting full, the loop uses `impl_->Flush()` instead of
 * `impl_->Write()` to upload data, and it also queries the status of the upload
 * after each `impl_->Flush()` call.
 *
 * If any of these operations fail the loop resumes the upload using a factory
 * function to create new `AsyncWriterConnection` instances. This class assumes
 * that the factory function implements the retry loop.
 *
 * If the factory function returns an error the loop ends.
 *
 * The loop also ends if there are no more bytes to send in the resend buffer.
 */
class AsyncWriterConnectionBuffered
    : public storage_experimental::AsyncWriterConnection {
 public:
  explicit AsyncWriterConnectionBuffered(
      WriterConnectionFactory factory,
      std::unique_ptr<storage_experimental::AsyncWriterConnection> impl,
      std::size_t buffer_size_lwm, std::size_t buffer_size_hwm)
      : state_(std::make_shared<AsyncWriterConnectionBufferedState>(
            std::move(factory), std::move(impl), buffer_size_lwm,
            buffer_size_hwm)) {}

  void Cancel() override { return state_->Cancel(); }

  std::string UploadId() const override { return state_->UploadId(); }

  absl::variant<std::int64_t, google::storage::v2::Object> PersistedState()
      const override {
    return state_->PersistedState();
  }

  future<Status> Write(storage_experimental::WritePayload p) override {
    return state_->Write(std::move(p));
  }

  future<StatusOr<google::storage::v2::Object>> Finalize(
      storage_experimental::WritePayload p) override {
    return state_->Finalize(std::move(p));
  }

  future<Status> Flush(storage_experimental::WritePayload p) override {
    return Write(std::move(p));
  }

  future<StatusOr<std::int64_t>> Query() override { return state_->Query(); }

  RpcMetadata GetRequestMetadata() override {
    return state_->GetRequestMetadata();
  }

 private:
  std::shared_ptr<AsyncWriterConnectionBufferedState> state_;
};

}  // namespace

std::unique_ptr<storage_experimental::AsyncWriterConnection>
MakeWriterConnectionBuffered(
    WriterConnectionFactory factory,
    std::unique_ptr<storage_experimental::AsyncWriterConnection> impl,
    Options const& options) {
  return absl::make_unique<AsyncWriterConnectionBuffered>(
      std::move(factory), std::move(impl),
      options.get<storage_experimental::BufferedUploadLwmOption>(),
      options.get<storage_experimental::BufferedUploadHwmOption>());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

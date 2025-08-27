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

#include "google/cloud/storage/internal/async/writer_connection_resumed.h"
#include "google/cloud/storage/internal/async/write_payload_impl.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "absl/strings/cord.h"
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

Status MakeRewindError(std::int64_t resend_offset, std::int64_t persisted_size,
                       internal::ErrorInfoBuilder eib) {
  auto const previous = std::to_string(resend_offset);
  auto const returned = std::to_string(persisted_size);
  return internal::InternalError(
      "server persisted_size rewind. This indicates a bug in the client library"
      " or the service.",
      std::move(eib)
          .WithMetadata("gcloud-cpp.storage.resend_offset", previous)
          .WithMetadata("gcloud-cpp.storage.persisted_size", returned));
}

Status MakeFastForwardError(std::int64_t resend_offset,
                            std::int64_t persisted_size,
                            internal::ErrorInfoBuilder eib) {
  auto const previous = std::to_string(resend_offset);
  auto const returned = std::to_string(persisted_size);
  return internal::InternalError(
      "server persisted_size too high. This can be caused by concurrent"
      " uploads using the same upload id. Most likely an application bug.",
      std::move(eib)
          .WithMetadata("gcloud-cpp.storage.resend_offset", previous)
          .WithMetadata("gcloud-cpp.storage.persisted_size", returned));
}

class AsyncWriterConnectionResumedState
    : public std::enable_shared_from_this<AsyncWriterConnectionResumedState> {
 public:
  AsyncWriterConnectionResumedState(
      WriterResultFactory factory,
      std::unique_ptr<storage_experimental::AsyncWriterConnection> impl,
      google::storage::v2::BidiWriteObjectRequest initial_request,
      std::shared_ptr<storage::internal::HashFunction> hash_function,
      Options const& options, std::size_t buffer_size_lwm,
      std::size_t buffer_size_hwm)
      : factory_(std::move(factory)),
        impl_(std::move(impl)),
        initial_request_(std::move(initial_request)),
        hash_function_(std::move(hash_function)),
        buffer_size_lwm_(buffer_size_lwm),
        buffer_size_hwm_(buffer_size_hwm) {
    finalized_future_ = finalized_.get_future();
    options_ = internal::MakeImmutableOptions(options);
    auto state = impl_->PersistedState();
    if (absl::holds_alternative<google::storage::v2::Object>(state)) {
      buffer_offset_ = absl::get<google::storage::v2::Object>(state).size();
    } else {
      buffer_offset_ = absl::get<std::int64_t>(state);
    }
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
    // Return the unique future associated with this finalization.
    return std::move(finalized_future_);
  }

  future<Status> Flush(storage_experimental::WritePayload const& p) {
    std::unique_lock<std::mutex> lk(mu_);
    // Create a new promise for this flush operation.
    promise<Status> current_flush_promise;
    auto f = current_flush_promise.get_future();
    pending_flush_promises_.push_back(std::move(current_flush_promise));

    resend_buffer_.Append(WritePayloadImpl::GetImpl(p));
    flush_ = true;
    HandleNewData(std::move(lk), true);
    // Return the future associated with the new promise.
    return f;
  }

  future<StatusOr<std::int64_t>> Query() {
    return Impl(std::unique_lock<std::mutex>(mu_))->Query();
  }

  RpcMetadata GetRequestMetadata() {
    return Impl(std::unique_lock<std::mutex>(mu_))->GetRequestMetadata();
  }

 private:
  std::weak_ptr<AsyncWriterConnectionResumedState> WeakFromThis() {
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

  future<Status> HandleNewData(std::unique_lock<std::mutex> lk,
                               bool flush = false) {
    if (!resume_status_.ok()) return make_ready_future(resume_status_);
    auto const buffer_size = resend_buffer_.size();
    flush_ = (buffer_size >= buffer_size_lwm_) || flush;
    auto result = make_ready_future(Status{});
    if (buffer_size >= buffer_size_hwm_) {
      auto p = promise<Status>();
      result = p.get_future();
      flush_handlers_.push_back(MakeLwmWaiter(std::move(p)));
    }
    StartWriting(std::move(lk));
    return result;
  }

  void StartWriting(std::unique_lock<std::mutex> lk) {
    if (writing_) return;
    WriteLoop(std::move(lk));
  }

  void WriteLoop(std::unique_lock<std::mutex> lk) {
    // Determine if there's data left to write *before* potentially finalizing.
    writing_ = write_offset_ < resend_buffer_.size();

    // If we are writing data, continue doing so.
    if (writing_) {
      // Still data to write, determine the next chunk.
      auto const n = resend_buffer_.size() - write_offset_;
      auto payload = resend_buffer_.Subcord(write_offset_, n);
      if (flush_) return FlushStep(std::move(lk), std::move(payload));
      return WriteStep(std::move(lk), std::move(payload));
    }

    // No data left to write (writing_ is false).
    // Check if we need to finalize (only if not already writing data AND not
    // already finalizing).
    if (finalize_ && !finalizing_) {
      // FinalizeStep will set the finalizing_ flag.
      return FinalizeStep(std::move(lk));
    }
    // If not finalizing, check if an empty flush is needed.
    if (flush_) {
      // Pass empty payload to FlushStep
      return FlushStep(std::move(lk), absl::Cord{});
    }

    // No data to write, not finalizing, not flushing. The loop can stop.
    // writing_ is already false.
  }

  // FinalizeStep is now called only when all data in resend_buffer_ is written.
  void FinalizeStep(std::unique_lock<std::mutex> lk) {
    // Check *under lock* if we are already finalizing.
    if (finalizing_) {
      // If another thread initiated FinalizeStep concurrently, just return.
      return;
    }
    // Mark that we are starting the finalization process.
    finalizing_ = true;
    auto impl = Impl(lk);
    lk.unlock();
    // Finalize with an empty payload.
    (void)impl->Finalize(storage_experimental::WritePayload{})
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
          if (auto self = w.lock()) {
            self->OnFlush(f.get(), size);
            return;
          }
        });
  }

  void OnFlush(Status const& result, std::size_t write_size) {
    if (!result.ok()) return Resume(std::move(result));
    std::unique_lock<std::mutex> lk(mu_);
    write_offset_ += write_size;
    auto impl = Impl(lk);
    lk.unlock();
    impl->Query().then([this, result, w = WeakFromThis()](auto f) {
      SetFlushed(std::unique_lock<std::mutex>(mu_), std::move(result));
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
      return SetError(
          std::move(lk),
          MakeRewindError(buffer_offset_, persisted_size, GCP_ERROR_INFO()));
    }
    auto const n = persisted_size - buffer_offset_;
    if (n > static_cast<std::int64_t>(resend_buffer_.size())) {
      return SetError(std::move(lk),
                      MakeFastForwardError(buffer_offset_, persisted_size,
                                           GCP_ERROR_INFO()));
    }
    resend_buffer_.RemovePrefix(static_cast<std::size_t>(n));
    buffer_offset_ = persisted_size;
    write_offset_ -= static_cast<std::size_t>(n);
    // If the buffer is small enough, collect all the handlers to notify them.
    auto const handlers = ClearHandlersIfEmpty(lk);
    WriteLoop(std::move(lk));
    // The notifications are deferred until the lock is released, as they might
    // call back and try to acquire the lock.
    for (auto const& h : handlers) {
      h->Execute(Status{});
    }
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

  void OnWrite(Status const& result, std::size_t write_size) {
    if (!result.ok()) return Resume(std::move(result));
    std::unique_lock<std::mutex> lk(mu_);
    write_offset_ += write_size;
    return WriteLoop(std::move(lk));
  }

  void Resume(Status const& s) {
    auto proto_status = ExtractGrpcStatus(s);
    auto request = google::storage::v2::BidiWriteObjectRequest{};
    auto spec = initial_request_.write_object_spec();
    auto& append_object_spec = *request.mutable_append_object_spec();
    append_object_spec.set_bucket(spec.resource().bucket());
    append_object_spec.set_object(spec.resource().name());
    append_object_spec.set_generation(first_response_.resource().generation());
    ApplyWriteRedirectErrors(append_object_spec, std::move(proto_status));

    // Capture the finalization state *before* starting the async resume.
    bool was_finalizing;
    {
      std::unique_lock<std::mutex> lk(mu_);
      was_finalizing = finalizing_;
      if (!s.ok() && cancelled_) {
        return SetError(std::move(lk), std::move(s));
      }
    }
    // Pass the original status `s` and `was_finalizing` to the callback.
    factory_(std::move(request))
        .then([s, was_finalizing, w = WeakFromThis()](auto f) {
          if (auto self = w.lock())
            return self->OnResume(s, was_finalizing, f.get());
        });
  }

  void OnResume(Status const& original_status, bool was_finalizing,
                StatusOr<WriteObject::WriteResult> res) {
    std::unique_lock<std::mutex> lk(mu_);

    if (was_finalizing) {
      // If resuming due to a finalization error, we *must* complete the
      // finalized_ promise now, based on the resume attempt's outcome.
      if (!res) {
        // The resume attempt itself failed. Use that error.
        return SetError(std::move(lk), std::move(res).status());
      }
      // Resume attempt succeeded, check the persisted state.
      auto state = impl_->PersistedState();
      if (absl::holds_alternative<google::storage::v2::Object>(state)) {
        // Resume found the object is finalized. Success.
        return SetFinalized(
            std::move(lk),
            absl::get<google::storage::v2::Object>(std::move(state)));
      }
      // Resume succeeded, but the object is still not finalized.
      // This means the original finalization attempt failed permanently.
      // Use the original status that triggered the resume. Reset finalizing_
      // before setting the error, as the attempt is now over.
      finalizing_ = false;
      return SetError(std::move(lk), std::move(original_status));
    }

    // Resume was *not* triggered by finalization failure.
    if (!res) {
      // Regular resume attempt failed.
      return SetError(std::move(lk), std::move(res).status());
    }
    // Regular resume attempt succeeded. Check state.
    auto state = impl_->PersistedState();
    if (absl::holds_alternative<google::storage::v2::Object>(state)) {
      // Found finalized object (maybe finalized concurrently or resumed).
      return SetFinalized(std::move(lk), absl::get<google::storage::v2::Object>(
                                             std::move(state)));
    }
    // Regular resume succeeded, object not finalized. Continue writing.
    auto persisted_offset = absl::get<std::int64_t>(state);
    impl_ = std::make_unique<AsyncWriterConnectionImpl>(
        options_, initial_request_, std::move(res->stream), hash_function_,
        persisted_offset, false);
    // OnQuery will restart the WriteLoop if necessary.
    OnQuery(std::move(lk), persisted_offset);
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
    finalizing_ = false;  // Reset finalizing flag
    flush_ = false;
    // Check if the promise has already been completed.
    if (finalized_promise_completed_) {
      lk.unlock();  // Release lock before returning
      return;
    }
    // Mark the promise as completed *before* moving it.
    finalized_promise_completed_ = true;
    auto handlers = ClearHandlers(lk);
    // Also clear any pending flush promises on success.
    auto pending_flushes = std::move(pending_flush_promises_);
    auto p = std::move(finalized_);  // Move the member promise
    lk.unlock();
    // Notify handlers and pending flushes *after* releasing the lock.
    for (auto& h : handlers) h->Execute(Status{});
    for (auto& pf : pending_flushes) pf.set_value(Status{});  // Success
    p.set_value(std::move(object));  // Set value on the moved promise
  }

  void SetFlushed(std::unique_lock<std::mutex> lk, Status const& result) {
    if (!result.ok()) return SetError(std::move(lk), std::move(result));
    // This flush step completed. We are no longer actively writing this chunk.
    // WriteLoop will determine if another flush/write is needed.
    writing_ = false;
    flush_ = false;  // Reset flush flag; WriteLoop may set it again.
    // Do NOT reset finalize_ or finalizing_ here.
    auto handlers = ClearHandlers(lk);
    // Dequeue the promise corresponding to an explicit Flush() call, if any.
    if (pending_flush_promises_.empty()) {
      // This can happen if SetError cleared the queue first, or if this
      // flush was triggered internally by buffer size (not by an explicit
      // Flush() call) and thus has no promise in the queue.
      lk.unlock();
      for (auto& h : handlers) h->Execute(Status{});
      return;
    }
    auto flushed = std::move(pending_flush_promises_.front());
    pending_flush_promises_.pop_front();

    lk.unlock();  // Unlock only once before notifying
    // Notify handlers and the specific flush promise *after* releasing the
    // lock.
    for (auto& h : handlers) h->Execute(Status{});
    flushed.set_value(result);
    // Restart the write loop ONLY if we are not already finalizing.
    // If finalizing_ is true, the completion will be handled by OnFinalize.
    std::unique_lock<std::mutex> loop_lk(mu_);
    if (!finalizing_) WriteLoop(std::move(loop_lk));
  }

  void SetError(std::unique_lock<std::mutex> lk, Status const& status) {
    resume_status_ = status;
    writing_ = false;
    finalize_ = false;
    finalizing_ = false;  // Reset finalizing flag
    flush_ = false;

    // Always clear handlers and pending flushes on error.
    auto handlers = ClearHandlers(lk);
    auto pending_flushes = std::move(pending_flush_promises_);

    // Check if the finalized promise has already been completed.
    if (finalized_promise_completed_) {
      // Finalized promise already set, just notify handlers and pending
      // flushes.
      lk.unlock();  // Release lock before notifying
      for (auto& h : handlers) h->Execute(status);
      for (auto& pf : pending_flushes) pf.set_value(status);
      return;
    }

    // Mark the finalized promise as completed *before* moving it under the
    // lock.
    finalized_promise_completed_ = true;
    // Move the finalized promise.
    auto p = std::move(finalized_);
    lk.unlock();  // Release lock before notifying

    // Notify handlers first.
    for (auto& h : handlers) h->Execute(status);
    // Set error on all pending flush promises.
    for (auto& pf : pending_flushes) {
      pf.set_value(status);
    }
    // Set error on the moved finalized promise *once*.
    p.set_value(status);
  }

  std::shared_ptr<storage_experimental::AsyncWriterConnection> Impl(
      std::unique_lock<std::mutex> const& /*lk*/) const {
    return impl_;
  }

  std::string UploadId(std::unique_lock<std::mutex> const& lk) const {
    return Impl(lk)->UploadId();
  }

  // Creates new `impl_` instances when needed.
  WriterResultFactory const factory_;

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

  // The initial request.
  google::storage::v2::BidiWriteObjectRequest initial_request_;

  std::shared_ptr<storage::internal::HashFunction> hash_function_;

  google::cloud::internal::ImmutableOptions options_;

  google::storage::v2::BidiWriteObjectResponse first_response_;

  // Request a server-side flush if the buffer goes over this threshold.
  std::size_t const buffer_size_lwm_;

  // Stop sending data if the buffer goes over this threshold. Only
  // start sending data again if the size goes below buffer_size_lwm_.
  std::size_t const buffer_size_hwm_;

  // The result of calling `Finalize()`. Note that only one such call is ever
  // made.
  promise<StatusOr<google::storage::v2::Object>> finalized_;

  // Retrieve the future in the constructor, as some operations reset
  // finalized_.
  future<StatusOr<google::storage::v2::Object>> finalized_future_;

  // Queue of promises for outstanding Flush() calls.
  std::deque<promise<Status>> pending_flush_promises_;

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

  // True if FinalizeStep has been initiated. Prevents re-entry.
  bool finalizing_ = false;

  // Tracks if the final promise (`finalized_`) has been completed.
  bool finalized_promise_completed_ = false;
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
class AsyncWriterConnectionResumed
    : public storage_experimental::AsyncWriterConnection {
 public:
  explicit AsyncWriterConnectionResumed(
      WriterResultFactory factory,
      std::unique_ptr<storage_experimental::AsyncWriterConnection> impl,
      google::storage::v2::BidiWriteObjectRequest initial_request,
      std::shared_ptr<storage::internal::HashFunction> hash_function,
      google::storage::v2::BidiWriteObjectResponse first_response,
      Options const& options)
      : state_(std::make_shared<AsyncWriterConnectionResumedState>(
            std::move(factory), std::move(impl), std::move(initial_request),
            std::move(hash_function), first_response, options,
            options.get<storage_experimental::BufferedUploadLwmOption>(),
            options.get<storage_experimental::BufferedUploadHwmOption>())) {}

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
    return state_->Flush(std::move(p));
  }

  future<StatusOr<std::int64_t>> Query() override { return state_->Query(); }

  RpcMetadata GetRequestMetadata() override {
    return state_->GetRequestMetadata();
  }

 private:
  std::shared_ptr<AsyncWriterConnectionResumedState> state_;
};

}  // namespace

std::unique_ptr<storage_experimental::AsyncWriterConnection>
MakeWriterConnectionResumed(
    WriterResultFactory factory,
    std::unique_ptr<storage_experimental::AsyncWriterConnection> impl,
    google::storage::v2::BidiWriteObjectRequest initial_request,
    std::shared_ptr<storage::internal::HashFunction> hash_function,
    google::storage::v2::BidiWriteObjectResponse first_response,
    Options const& options) {
  return absl::make_unique<AsyncWriterConnectionResumed>(
      std::move(factory), std::move(impl), std::move(initial_request),
      std::move(hash_function), std::move(first_response), std::move(options));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

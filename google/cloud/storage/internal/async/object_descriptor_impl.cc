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

#include "google/cloud/storage/internal/async/object_descriptor_impl.h"
#include "google/cloud/storage/async/options.h"
#include "google/cloud/storage/internal/async/handle_redirect_error.h"
#include "google/cloud/storage/internal/async/multi_stream_manager.h"
#include "google/cloud/storage/internal/async/object_descriptor_reader_tracing.h"
#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/storage/internal/hash_function_impl.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/opentelemetry.h"
#include <google/rpc/status.pb.h>
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

ObjectDescriptorImpl::ObjectDescriptorImpl(
    std::unique_ptr<storage_experimental::ResumePolicy> resume_policy,
    OpenStreamFactory make_stream,
    google::storage::v2::BidiReadObjectSpec read_object_spec,
    std::shared_ptr<OpenStream> stream, Options options)
    : resume_policy_prototype_(std::move(resume_policy)),
      make_stream_(std::move(make_stream)),
      read_object_spec_(std::move(read_object_spec)),
      options_(std::move(options)) {
  stream_manager_ = std::make_unique<StreamManager>(
      []() -> std::shared_ptr<ReadStream> { return nullptr; },
      std::make_shared<ReadStream>(std::move(stream),
                                   resume_policy_prototype_->clone()));
}

ObjectDescriptorImpl::~ObjectDescriptorImpl() { Cancel(); }

void ObjectDescriptorImpl::Start(
    google::storage::v2::BidiReadObjectResponse first_response) {
  std::unique_lock<std::mutex> lk(mu_);
  auto it = stream_manager_->GetLastStream();
  // Unlock and start the Read loop first.
  lk.unlock();
  OnRead(it, std::move(first_response));
  // Acquire lock and queue the background stream.
  lk.lock();
  AssurePendingStreamQueued();
}

void ObjectDescriptorImpl::Cancel() {
  std::unique_lock<std::mutex> lk(mu_);
  cancelled_ = true;
  if (stream_manager_) stream_manager_->CancelAll();
  if (pending_stream_.valid()) pending_stream_.cancel();
}

absl::optional<google::storage::v2::Object> ObjectDescriptorImpl::metadata()
    const {
  std::unique_lock<std::mutex> lk(mu_);
  return metadata_;
}

void ObjectDescriptorImpl::AssurePendingStreamQueued() {
  if (pending_stream_.valid()) return;
  auto request = google::storage::v2::BidiReadObjectRequest{};

  *request.mutable_read_object_spec() = read_object_spec_;
  pending_stream_ = make_stream_(std::move(request));
}

void ObjectDescriptorImpl::MakeSubsequentStream() {
  std::unique_lock<std::mutex> lk(mu_);
  // Reuse an idle stream if possible.
  if (stream_manager_->ReuseIdleStreamToBack(
          [](StreamManager::Stream const& s) {
            auto const* rs = s.stream.get();
            return rs && s.active_ranges.empty() && !rs->write_pending;
          })) {
    return;
  }
  // Proactively create a new stream if needed.
  AssurePendingStreamQueued();
  auto stream_future = std::move(pending_stream_);
  lk.unlock();

  //  Wait for the stream to be created.
  auto stream_result = stream_future.get();
  if (!stream_result) {
    // Stream creation failed.
    // The next call to AssurePendingStreamQueued will retry creation.
    return;
  }

  lk.lock();
  if (cancelled_) return;
  auto read_stream = std::make_shared<ReadStream>(
      std::move(stream_result->stream), resume_policy_prototype_->clone());
  auto new_it = stream_manager_->AddStream(std::move(read_stream));
  // Now that we consumed pending_stream_, queue the next one immediately.
  AssurePendingStreamQueued();

  lk.unlock();
  OnRead(new_it, std::move(stream_result->first_response));
}

std::unique_ptr<storage_experimental::AsyncReaderConnection>
ObjectDescriptorImpl::Read(ReadParams p) {
  std::shared_ptr<storage::internal::HashFunction> hash_function =
      std::shared_ptr<storage::internal::HashFunction>(
          storage::internal::CreateNullHashFunction());
  if (options_.has<storage_experimental::EnableCrc32cValidationOption>()) {
    hash_function =
        std::make_shared<storage::internal::Crc32cMessageHashFunction>(
            storage::internal::CreateNullHashFunction());
  }
  auto range = std::make_shared<ReadRange>(p.start, p.length, hash_function);

  std::unique_lock<std::mutex> lk(mu_);
  if (stream_manager_->Empty()) {
    lk.unlock();
    range->OnFinish(Status(StatusCode::kFailedPrecondition,
                           "Cannot read object, all streams failed"));
    if (!internal::TracingEnabled(options_)) {
      return std::unique_ptr<storage_experimental::AsyncReaderConnection>(
          std::make_unique<ObjectDescriptorReader>(std::move(range)));
    }
    return MakeTracingObjectDescriptorReader(std::move(range));
  }

  auto it = stream_manager_->GetLeastBusyStream();
  auto const id = ++read_id_generator_;
  it->active_ranges.emplace(id, range);
  auto& read_range = *it->stream->next_request.add_read_ranges();
  read_range.set_read_id(id);
  read_range.set_read_offset(p.start);
  read_range.set_read_length(p.length);
  Flush(std::move(lk), it);

  if (!internal::TracingEnabled(options_)) {
    return std::unique_ptr<storage_experimental::AsyncReaderConnection>(
        std::make_unique<ObjectDescriptorReader>(std::move(range)));
  }

  return MakeTracingObjectDescriptorReader(std::move(range));
}

void ObjectDescriptorImpl::Flush(std::unique_lock<std::mutex> lk,
                                 StreamIterator it) {
  if (it->stream->write_pending ||
      it->stream->next_request.read_ranges().empty()) {
    return;
  }
  it->stream->write_pending = true;
  google::storage::v2::BidiReadObjectRequest request;
  request.Swap(&it->stream->next_request);

  // Assign CurrentStream to a temporary variable to prevent
  // lifetime extension which can cause the lock to be held until the
  // end of the block.
  auto current_stream = it->stream->stream_;
  lk.unlock();
  current_stream->Write(std::move(request))
      .then([w = WeakFromThis(), it](auto f) {
        if (auto self = w.lock()) self->OnWrite(it, f.get());
      });
}

void ObjectDescriptorImpl::OnWrite(StreamIterator it, bool ok) {
  std::unique_lock<std::mutex> lk(mu_);
  if (!ok) return DoFinish(std::move(lk), it);
  it->stream->write_pending = false;
  Flush(std::move(lk), it);
}

void ObjectDescriptorImpl::DoRead(std::unique_lock<std::mutex> lk,
                                  StreamIterator it) {
  if (it->stream->read_pending) return;
  it->stream->read_pending = true;

  // Assign CurrentStream to a temporary variable to prevent
  // lifetime extension which can cause the lock to be held until the
  // end of the block.
  auto current_stream = it->stream->stream_;
  lk.unlock();
  current_stream->Read().then([w = WeakFromThis(), it](auto f) {
    if (auto self = w.lock()) self->OnRead(it, f.get());
  });
}

void ObjectDescriptorImpl::OnRead(
    StreamIterator it,
    absl::optional<google::storage::v2::BidiReadObjectResponse> response) {
  std::unique_lock<std::mutex> lk(mu_);
  it->stream->read_pending = false;

  if (!response) return DoFinish(std::move(lk), it);
  if (response->has_metadata()) {
    metadata_ = std::move(*response->mutable_metadata());
  }
  if (response->has_read_handle()) {
    *read_object_spec_.mutable_read_handle() =
        std::move(*response->mutable_read_handle());
  }
  auto copy = it->active_ranges;
  // Release the lock while notifying the ranges. The notifications may trigger
  // application code, and that code may callback on this class.
  lk.unlock();
  for (auto& range_data : *response->mutable_object_data_ranges()) {
    auto id = range_data.read_range().read_id();
    auto const l = copy.find(id);
    if (l == copy.end()) continue;
    // TODO(#15104) - Consider returning if the range is done, and then
    // skipping CleanupDoneRanges().
    l->second->OnRead(std::move(range_data));
  }
  lk.lock();
  stream_manager_->CleanupDoneRanges(it);
  DoRead(std::move(lk), it);
}

void ObjectDescriptorImpl::DoFinish(std::unique_lock<std::mutex> lk,
                                    StreamIterator it) {
  it->stream->read_pending = false;
  // Assign CurrentStream to a temporary variable to prevent
  // lifetime extension which can cause the lock to be held until the
  // end of the block.
  auto current_stream = it->stream->stream_;
  lk.unlock();
  auto pending = current_stream->Finish();
  if (!pending.valid()) return;
  pending.then([w = WeakFromThis(), it](auto f) {
    if (auto self = w.lock()) self->OnFinish(it, f.get());
  });
}

void ObjectDescriptorImpl::OnFinish(StreamIterator it, Status const& status) {
  auto proto_status = ExtractGrpcStatus(status);

  if (IsResumable(it, status, proto_status)) return Resume(it, proto_status);
  std::unique_lock<std::mutex> lk(mu_);
  stream_manager_->RemoveStreamAndNotifyRanges(it, status);
  // Since a stream died, we might want to ensure a replacement is queued.
  AssurePendingStreamQueued();
}

void ObjectDescriptorImpl::Resume(StreamIterator it,
                                  google::rpc::Status const& proto_status) {
  std::unique_lock<std::mutex> lk(mu_);
  // This call needs to happen inside the lock, as it may modify
  // `read_object_spec_`.
  ApplyRedirectErrors(read_object_spec_, proto_status);
  auto request = google::storage::v2::BidiReadObjectRequest{};
  *request.mutable_read_object_spec() = read_object_spec_;
  for (auto const& kv : it->active_ranges) {
    auto range = kv.second->RangeForResume(kv.first);
    if (!range) continue;
    *request.add_read_ranges() = *std::move(range);
  }
  lk.unlock();
  make_stream_(std::move(request)).then([w = WeakFromThis(), it](auto f) {
    if (auto self = w.lock()) self->OnResume(it, f.get());
  });
}

void ObjectDescriptorImpl::OnResume(StreamIterator it,
                                    StatusOr<OpenStreamResult> result) {
  if (!result) return OnFinish(it, std::move(result).status());
  std::unique_lock<std::mutex> lk(mu_);
  if (cancelled_) return;

  it->stream = std::make_shared<ReadStream>(std::move(result->stream),
                                            resume_policy_prototype_->clone());
  it->stream->write_pending = false;
  it->stream->read_pending = false;

  // TODO(#15105) - this should be done without release the lock.
  Flush(std::move(lk), it);
  OnRead(it, std::move(result->first_response));
}

bool ObjectDescriptorImpl::IsResumable(
    StreamIterator it, Status const& status,
    google::rpc::Status const& proto_status) {
  std::unique_lock<std::mutex> lk(mu_);
  for (auto const& any : proto_status.details()) {
    auto error = google::storage::v2::BidiReadObjectError{};
    if (!any.UnpackTo(&error)) continue;

    std::vector<std::pair<std::int64_t, Status>> notify;
    for (auto const& re : error.read_range_errors()) {
      if (it->active_ranges.count(re.read_id())) {
        notify.emplace_back(re.read_id(), MakeStatusFromRpcError(re.status()));
      }
    }
    if (notify.empty()) continue;

    auto copy = it->active_ranges;
    lk.unlock();
    for (auto const& p : notify) {
      auto l = copy.find(p.first);
      if (l != copy.end()) l->second->OnFinish(p.second);
    }
    lk.lock();
    stream_manager_->CleanupDoneRanges(it);
    return true;
  }
  return it->stream->resume_policy_->OnFinish(status) ==
         storage_experimental::ResumePolicy::kContinue;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

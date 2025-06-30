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
    : resume_policy_(std::move(resume_policy)),
      make_stream_(std::move(make_stream)),
      read_object_spec_(std::move(read_object_spec)),
      options_(std::move(options)),
      active_stream_(0),
      streams_{std::move(stream)} {}

ObjectDescriptorImpl::~ObjectDescriptorImpl() {
  for (auto const& stream : streams_) {
    stream->Cancel();
  }
}

void ObjectDescriptorImpl::Start(
    google::storage::v2::BidiReadObjectResponse first_response) {
  OnRead(std::move(first_response));
}

void ObjectDescriptorImpl::Cancel() {
  for (auto const& stream : streams_) {
    stream->Cancel();
  }
}

absl::optional<google::storage::v2::Object> ObjectDescriptorImpl::metadata()
    const {
  std::unique_lock<std::mutex> lk(mu_);
  return metadata_;
}

void ObjectDescriptorImpl::MakeSubsequentStream() {
  auto request = google::storage::v2::BidiReadObjectRequest{};

  *request.mutable_read_object_spec() = read_object_spec_;
  auto stream_result = make_stream_(std::move(request)).get();
  auto stream = std::move(stream_result->stream);

  std::unique_lock<std::mutex> lk(mu_);
  active_stream_ = streams_.size();
  streams_.push_back(std::move(stream));
  lk.unlock();

  Start(std::move(stream_result->first_response));
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
  auto const id = ++read_id_generator_;
  active_ranges_.emplace(id, range);
  auto& read_range = *next_request_.add_read_ranges();
  read_range.set_read_id(id);
  read_range.set_read_offset(p.start);
  read_range.set_read_length(p.length);
  Flush(std::move(lk));

  if (!internal::TracingEnabled(options_)) {
    return std::unique_ptr<storage_experimental::AsyncReaderConnection>(
        std::make_unique<ObjectDescriptorReader>(std::move(range)));
  }

  return MakeTracingObjectDescriptorReader(std::move(range));
}

void ObjectDescriptorImpl::Flush(std::unique_lock<std::mutex> lk) {
  if (write_pending_ || next_request_.read_ranges().empty()) return;
  write_pending_ = true;
  google::storage::v2::BidiReadObjectRequest request;
  request.Swap(&next_request_);

  // Assign CurrentStream to a temporary variable to prevent
  // lifetime extension which can cause the lock to be held until the
  // end of the block.
  auto current_stream = CurrentStream(std::move(lk));
  current_stream->Write(std::move(request)).then([w = WeakFromThis()](auto f) {
    if (auto self = w.lock()) self->OnWrite(f.get());
  });
}

void ObjectDescriptorImpl::OnWrite(bool ok) {
  std::unique_lock<std::mutex> lk(mu_);
  if (!ok) return DoFinish(std::move(lk));
  write_pending_ = false;
  Flush(std::move(lk));
}

void ObjectDescriptorImpl::DoRead(std::unique_lock<std::mutex> lk) {
  // Assign CurrentStream to a temporary variable to prevent
  // lifetime extension which can cause the lock to be held until the
  // end of the block.
  auto current_stream = CurrentStream(std::move(lk));
  current_stream->Read().then([w = WeakFromThis()](auto f) {
    if (auto self = w.lock()) self->OnRead(f.get());
  });
}

void ObjectDescriptorImpl::OnRead(
    absl::optional<google::storage::v2::BidiReadObjectResponse> response) {
  std::unique_lock<std::mutex> lk(mu_);
  if (!response) return DoFinish(std::move(lk));
  if (response->has_metadata()) {
    metadata_ = std::move(*response->mutable_metadata());
  }
  if (response->has_read_handle()) {
    *read_object_spec_.mutable_read_handle() =
        std::move(*response->mutable_read_handle());
  }
  auto copy = CopyActiveRanges(lk);
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
  CleanupDoneRanges(lk);
  DoRead(std::move(lk));
}

void ObjectDescriptorImpl::CleanupDoneRanges(
    std::unique_lock<std::mutex> const&) {
  for (auto i = active_ranges_.begin(); i != active_ranges_.end();) {
    if (i->second->IsDone()) {
      i = active_ranges_.erase(i);
    } else {
      ++i;
    }
  }
}

void ObjectDescriptorImpl::DoFinish(std::unique_lock<std::mutex> lk) {
  // Assign CurrentStream to a temporary variable to prevent
  // lifetime extension which can cause the lock to be held until the
  // end of the block.
  auto current_stream = CurrentStream(std::move(lk));
  auto pending = current_stream->Finish();
  if (!pending.valid()) return;
  pending.then([w = WeakFromThis()](auto f) {
    if (auto self = w.lock()) self->OnFinish(f.get());
  });
}

void ObjectDescriptorImpl::OnFinish(Status const& status) {
  auto proto_status = ExtractGrpcStatus(status);

  if (IsResumable(status, proto_status)) return Resume(proto_status);
  std::unique_lock<std::mutex> lk(mu_);
  auto copy = CopyActiveRanges(std::move(lk));
  for (auto const& kv : copy) {
    kv.second->OnFinish(status);
  }
}

void ObjectDescriptorImpl::Resume(google::rpc::Status const& proto_status) {
  std::unique_lock<std::mutex> lk(mu_);
  // This call needs to happen inside the lock, as it may modify
  // `read_object_spec_`.
  ApplyRedirectErrors(read_object_spec_, proto_status);
  auto request = google::storage::v2::BidiReadObjectRequest{};
  *request.mutable_read_object_spec() = read_object_spec_;
  for (auto const& kv : active_ranges_) {
    auto range = kv.second->RangeForResume(kv.first);
    if (!range) continue;
    *request.add_read_ranges() = *std::move(range);
  }
  write_pending_ = true;
  lk.unlock();
  make_stream_(std::move(request)).then([w = WeakFromThis()](auto f) {
    if (auto self = w.lock()) self->OnResume(f.get());
  });
}

void ObjectDescriptorImpl::OnResume(StatusOr<OpenStreamResult> result) {
  if (!result) return OnFinish(std::move(result).status());
  std::unique_lock<std::mutex> lk(mu_);
  streams_[0] = std::move(result->stream);
  // TODO(#15105) - this should be done without release the lock.
  Flush(std::move(lk));
  OnRead(std::move(result->first_response));
}

bool ObjectDescriptorImpl::IsResumable(
    Status const& status, google::rpc::Status const& proto_status) {
  for (auto const& any : proto_status.details()) {
    auto error = google::storage::v2::BidiReadObjectError{};
    if (!any.UnpackTo(&error)) continue;
    auto ranges = CopyActiveRanges();
    for (auto const& range : CopyActiveRanges()) {
      for (auto const& range_error : error.read_range_errors()) {
        if (range.first != range_error.read_id()) continue;
        range.second->OnFinish(MakeStatusFromRpcError(range_error.status()));
      }
    }
    CleanupDoneRanges(std::unique_lock<std::mutex>(mu_));
    return true;
  }

  return resume_policy_->OnFinish(status) ==
         storage_experimental::ResumePolicy::kContinue;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

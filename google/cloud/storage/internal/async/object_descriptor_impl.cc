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
#include "google/cloud/internal/status_payload_keys.h"
#include <google/rpc/status.pb.h>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

ObjectDescriptorImpl::ObjectDescriptorImpl(
    std::unique_ptr<storage_experimental::ResumePolicy> resume_policy,
    OpenStreamFactory make_stream,
    google::storage::v2::BidiReadObjectSpec read_object_spec,
    std::shared_ptr<OpenStream> stream)
    : resume_policy_(std::move(resume_policy)),
      make_stream_(std::move(make_stream)),
      read_object_spec_(std::move(read_object_spec)),
      stream_(std::move(stream)) {}

ObjectDescriptorImpl::~ObjectDescriptorImpl() { stream_->Cancel(); }

void ObjectDescriptorImpl::Start(
    google::storage::v2::BidiReadObjectResponse first_response) {
  OnRead(std::move(first_response));
}

void ObjectDescriptorImpl::Cancel() { return stream_->Cancel(); }

absl::optional<google::storage::v2::Object> ObjectDescriptorImpl::metadata()
    const {
  std::unique_lock<std::mutex> lk(mu_);
  return metadata_;
}

std::unique_ptr<storage_experimental::AsyncReaderConnection>
ObjectDescriptorImpl::Read(ReadParams p) {
  auto range = std::make_shared<ReadRange>(p.start, p.limit);

  std::unique_lock<std::mutex> lk(mu_);
  auto const id = ++read_id_generator_;
  active_ranges_.emplace(id, range);
  auto& read_range = *next_request_.add_read_ranges();
  read_range.set_read_id(id);
  read_range.set_read_offset(p.start);
  read_range.set_read_limit(p.limit);
  Flush(std::move(lk));
  return std::unique_ptr<storage_experimental::AsyncReaderConnection>(
      std::make_unique<ObjectDescriptorReader>(std::move(range)));
}

void ObjectDescriptorImpl::Flush(std::unique_lock<std::mutex> lk) {
  if (write_pending_ || next_request_.read_ranges().empty()) return;
  write_pending_ = true;
  google::storage::v2::BidiReadObjectRequest request;
  request.Swap(&next_request_);
  CurrentStream(std::move(lk))
      ->Write(std::move(request))
      .then([w = WeakFromThis()](auto f) {
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
  CurrentStream(std::move(lk))->Read().then([w = WeakFromThis()](auto f) {
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
    // TODO(#34) - Consider returning if the range is done, and then
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
  CurrentStream(std::move(lk))->Finish().then([w = WeakFromThis()](auto f) {
    if (auto self = w.lock()) self->OnFinish(f.get());
  });
}

void ObjectDescriptorImpl::OnFinish(Status const& status) {
  using Action = storage_experimental::ResumePolicy::Action;
  auto const action = resume_policy_->OnFinish(status);
  if (action == Action::kContinue) return Resume(status);
  std::unique_lock<std::mutex> lk(mu_);
  auto copy = CopyActiveRanges(std::move(lk));
  for (auto const& kv : copy) {
    kv.second->OnFinish(status);
  }
}

void ObjectDescriptorImpl::Resume(Status const& status) {
  auto proto_status = google::rpc::Status{};
  auto payload = google::cloud::internal::GetPayload(
      status, google::cloud::internal::StatusPayloadGrpcProto());
  if (payload) proto_status.ParseFromString(*payload);

  std::unique_lock<std::mutex> lk(mu_);
  // This loop needs to happen inside the lock, as it may modify
  // `read_object_spec_`.
  for (auto const& any : proto_status.details()) {
    auto error = google::storage::v2::BidiReadObjectRedirectedError{};
    if (!any.UnpackTo(&error)) continue;
    *read_object_spec_.mutable_read_handle() =
        std::move(*error.mutable_read_handle());
    *read_object_spec_.mutable_routing_token() =
        std::move(*error.mutable_routing_token());
  }
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
  stream_ = std::move(result->stream);
  // TODO(#36) - this should be done without release the lock.
  Flush(std::move(lk));
  OnRead(std::move(result->first_response));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

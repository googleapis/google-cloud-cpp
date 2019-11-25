// Copyright 2019 Google LLC
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

#include "google/cloud/storage/parallel_upload.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

class ParallelObjectWriteStreambuf : public ObjectWriteStreambuf {
 public:
  ParallelObjectWriteStreambuf(
      std::shared_ptr<NonResumableParallelUploadState::Impl> state,
      std::size_t stream_idx,
      std::unique_ptr<ResumableUploadSession> upload_session,
      std::size_t max_buffer_size,
      std::unique_ptr<HashValidator> hash_validator)
      : ObjectWriteStreambuf(std::move(upload_session), max_buffer_size,
                             std::move(hash_validator)),
        state_(std::move(state)),
        stream_idx_(stream_idx) {}

  StatusOr<internal::ResumableUploadResponse> Close() override {
    auto res = this->ObjectWriteStreambuf::Close();
    state_->StreamFinished(stream_idx_, res);
    return res;
  }

 private:
  std::shared_ptr<NonResumableParallelUploadState::Impl> state_;
  std::size_t stream_idx_;
};

}  // namespace internal

NonResumableParallelUploadState::Impl::Impl(
    std::unique_ptr<internal::ScopedDeleter> deleter, Composer composer)
    : deleter_(std::move(deleter)),
      composer_(std::move(composer)),
      finished_{},
      num_unfinished_streams_{} {}

NonResumableParallelUploadState::Impl::~Impl() { WaitForCompletion(); }

StatusOr<ObjectWriteStream> NonResumableParallelUploadState::Impl::CreateStream(
    internal::RawClient& raw_client,
    internal::ResumableUploadRequest const& request) {
  auto session = raw_client.CreateResumableSession(request);
  std::unique_lock<std::mutex> lk(mu_);
  if (!session) {
    // Preserve the first error.
    res_ = session.status();
    return std::move(session).status();
  }
  return ObjectWriteStream(google::cloud::internal::make_unique<
                           internal::ParallelObjectWriteStreambuf>(
      shared_from_this(), num_unfinished_streams_++, *std::move(session),
      raw_client.client_options().upload_buffer_size(),
      internal::CreateHashValidator(request)));
}

Status NonResumableParallelUploadState::Impl::EagerCleanup() {
  std::unique_lock<std::mutex> lk(mu_);
  if (!finished_) {
    return Status(StatusCode::kFailedPrecondition,
                  "Attempted to cleanup parallel upload state while it is "
                  "still in progress");
  }
  // Make sure that only one thread actually interacts with the deleter.
  if (deleter_) {
    cleanup_status_ = deleter_->ExecuteDelete();
    deleter_.reset();
  }
  return cleanup_status_;
}

void NonResumableParallelUploadState::Impl::StreamFinished(
    std::size_t stream_idx,
    StatusOr<internal::ResumableUploadResponse> const& response) {
  std::unique_lock<std::mutex> lk(mu_);

  --num_unfinished_streams_;
  if (!response) {
    // The upload failed, we don't event need to clean this up.
    if (!res_) {
      // Preserve the first error.
      res_ = response.status();
    }
  } else {
    ObjectMetadata metadata = *response->payload;
    to_compose_.resize(std::max(to_compose_.size(), stream_idx + 1));
    to_compose_[stream_idx] =
        ComposeSourceObject{metadata.name(), metadata.generation(), {}};
    deleter_->Add(std::move(metadata));
  }
  if (num_unfinished_streams_ > 0) {
    return;
  }
  if (!res_) {
    // only execute ComposeMany if all the streams succeeded.
    lk.unlock();
    res_ = composer_(std::move(to_compose_));
    lk.lock();
  }
  // All done, wake up whomever is waiting.
  finished_ = true;
  cv_.notify_all();
}

StatusOr<ObjectMetadata>
NonResumableParallelUploadState::Impl::WaitForCompletion() const {
  std::unique_lock<std::mutex> lk(mu_);
  cv_.wait(lk, [this] { return finished_; });
  return *res_;
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

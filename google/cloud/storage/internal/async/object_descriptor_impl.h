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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OBJECT_DESCRIPTOR_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OBJECT_DESCRIPTOR_IMPL_H

#include "google/cloud/storage/async/object_descriptor_connection.h"
#include "google/cloud/storage/async/resume_policy.h"
#include "google/cloud/storage/internal/async/object_descriptor_reader.h"
#include "google/cloud/storage/internal/async/open_stream.h"
#include "google/cloud/storage/internal/async/read_range.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <google/storage/v2/storage.pb.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class ObjectDescriptorImpl
    : public storage_experimental::ObjectDescriptorConnection,
      public std::enable_shared_from_this<ObjectDescriptorImpl> {
 public:
  ObjectDescriptorImpl(
      std::unique_ptr<storage_experimental::ResumePolicy> resume_policy,
      OpenStreamFactory make_stream,
      google::storage::v2::BidiReadObjectSpec read_object_spec,
      std::shared_ptr<OpenStream> stream);
  ~ObjectDescriptorImpl() override;

  // Start the read loop.
  void Start(google::storage::v2::BidiReadObjectResponse first_response);

  // Cancel the underlying RPC and stop the resume loop.
  void Cancel();

  // Return the object metadata. This is only available after the first `Read()`
  // returns.
  absl::optional<google::storage::v2::Object> metadata() const override;

  // Start a new ranged read.
  std::unique_ptr<storage_experimental::AsyncReaderConnection> Read(
      ReadParams p) override;

 private:
  std::weak_ptr<ObjectDescriptorImpl> WeakFromThis() {
    return shared_from_this();
  }

  // This may seem expensive, but it is less bug-prone than iterating over
  // the map with the lock held.
  auto CopyActiveRanges(std::unique_lock<std::mutex> const&) const {
    return active_ranges_;
  }

  auto CopyActiveRanges() const {
    return CopyActiveRanges(std::unique_lock<std::mutex>(mu_));
  }

  auto CurrentStream(std::unique_lock<std::mutex>) const { return stream_; }

  void Flush(std::unique_lock<std::mutex> lk);
  void OnWrite(bool ok);
  void DoRead(std::unique_lock<std::mutex>);
  void OnRead(
      absl::optional<google::storage::v2::BidiReadObjectResponse> response);
  void CleanupDoneRanges(std::unique_lock<std::mutex> const&);
  void DoFinish(std::unique_lock<std::mutex>);
  void OnFinish(Status const& status);
  void Resume(Status const& status);
  void OnResume(StatusOr<OpenStreamResult> result);

  std::unique_ptr<storage_experimental::ResumePolicy> resume_policy_;
  OpenStreamFactory make_stream_;

  mutable std::mutex mu_;
  google::storage::v2::BidiReadObjectSpec read_object_spec_;
  std::shared_ptr<OpenStream> stream_;
  absl::optional<google::storage::v2::Object> metadata_;
  std::int64_t read_id_generator_ = 0;
  bool write_pending_ = false;
  google::storage::v2::BidiReadObjectRequest next_request_;

  std::unordered_map<std::int64_t, std::shared_ptr<ReadRange>> active_ranges_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OBJECT_DESCRIPTOR_IMPL_H

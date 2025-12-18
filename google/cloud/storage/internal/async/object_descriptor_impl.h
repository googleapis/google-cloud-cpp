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
#include "google/cloud/storage/internal/async/multi_stream_manager.h"
#include "google/cloud/storage/internal/async/object_descriptor_reader.h"
#include "google/cloud/storage/internal/async/open_stream.h"
#include "google/cloud/storage/internal/async/read_range.h"
#include "google/cloud/storage/options.h"
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

class ReadStream : public storage_internal::StreamBase {
 public:
  ReadStream(std::shared_ptr<OpenStream> stream,
             std::unique_ptr<storage_experimental::ResumePolicy> resume_policy)
      : stream_(std::move(stream)), resume_policy_(std::move(resume_policy)) {}

  void Cancel() override {
    if (stream_) stream_->Cancel();
  }

  std::shared_ptr<OpenStream> stream_;
  std::unique_ptr<storage_experimental::ResumePolicy> resume_policy_;
  google::storage::v2::BidiReadObjectRequest next_request_;
  bool write_pending_ = false;
  bool read_pending_ = false;
};

class ObjectDescriptorImpl
    : public storage_experimental::ObjectDescriptorConnection,
      public std::enable_shared_from_this<ObjectDescriptorImpl> {
 private:
  using StreamManager = MultiStreamManager<ReadStream, ReadRange>;
  using StreamIterator = StreamManager::StreamIterator;

 public:
  ObjectDescriptorImpl(
      std::unique_ptr<storage_experimental::ResumePolicy> resume_policy,
      OpenStreamFactory make_stream,
      google::storage::v2::BidiReadObjectSpec read_object_spec,
      std::shared_ptr<OpenStream> stream, Options options = {});
  ~ObjectDescriptorImpl() override;

  // Start the read loop.
  void Start(google::storage::v2::BidiReadObjectResponse first_response);

  // Cancel the underlying RPC and stop the resume loop.
  void Cancel();

  Options options() const override { return options_; }

  // Return the object metadata. This is only available after the first `Read()`
  // returns.
  absl::optional<google::storage::v2::Object> metadata() const override;

  // Start a new ranged read.
  std::unique_ptr<storage_experimental::AsyncReaderConnection> Read(
      ReadParams p) override;

  void MakeSubsequentStream() override;

 private:
  std::weak_ptr<ObjectDescriptorImpl> WeakFromThis() {
    return shared_from_this();
  }

  // Logic to ensure a background stream is always connecting.
  void AssurePendingStreamQueued();

  void Flush(std::unique_lock<std::mutex> lk, StreamIterator it);
  void OnWrite(StreamIterator it, bool ok);
  void DoRead(std::unique_lock<std::mutex> lk, StreamIterator it);
  void OnRead(
      StreamIterator it,
      absl::optional<google::storage::v2::BidiReadObjectResponse> response);
  void DoFinish(std::unique_lock<std::mutex> lk, StreamIterator it);
  void OnFinish(StreamIterator it, Status const& status);
  void Resume(StreamIterator it, google::rpc::Status const& proto_status);
  void OnResume(StreamIterator it, StatusOr<OpenStreamResult> result);
  bool IsResumable(StreamIterator it, Status const& status,
                   google::rpc::Status const& proto_status);

  std::unique_ptr<storage_experimental::ResumePolicy> resume_policy_prototype_;
  OpenStreamFactory make_stream_;

  mutable std::mutex mu_;
  google::storage::v2::BidiReadObjectSpec read_object_spec_;
  absl::optional<google::storage::v2::Object> metadata_;
  std::int64_t read_id_generator_ = 0;

  Options options_;
  std::unique_ptr<StreamManager> stream_manager_;
  // The future for the proactive background stream.
  google::cloud::future<
      google::cloud::StatusOr<storage_internal::OpenStreamResult>>
      pending_stream_;
  bool cancelled_ = false;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OBJECT_DESCRIPTOR_IMPL_H

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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_OBJECT_READ_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_OBJECT_READ_SOURCE_H

#include "google/cloud/storage/internal/object_read_source.h"
#include "google/cloud/storage/internal/retry_client.h"
#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

enum OffsetDirection { kFromBeginning, kFromEnd };

/**
 * A data source for ObjectReadStreambuf.
 *
 * This object represents an open download stream. It is an abstract class
 * because (a) we do not want to expose CURL types in the public headers, and
 * (b) we want to break the functionality for retry vs. simple downloads in
 * different classes.
 */
class RetryObjectReadSource : public ObjectReadSource {
 public:
  RetryObjectReadSource(std::shared_ptr<RetryClient> client,
                        ReadObjectRangeRequest request,
                        std::unique_ptr<ObjectReadSource> child,
                        std::unique_ptr<RetryPolicy> retry_policy,
                        std::unique_ptr<BackoffPolicy> backoff_policy);

  bool IsOpen() const override { return child_ && child_->IsOpen(); }
  StatusOr<HttpResponse> Close() override { return child_->Close(); }
  StatusOr<ReadSourceResult> Read(char* buf, std::size_t n) override;

 private:
  std::shared_ptr<RetryClient> client_;
  ReadObjectRangeRequest request_;
  std::unique_ptr<ObjectReadSource> child_;
  optional<std::int64_t> generation_;
  std::unique_ptr<RetryPolicy const> retry_policy_prototype_;
  std::unique_ptr<BackoffPolicy const> backoff_policy_prototype_;
  OffsetDirection offset_direction_;
  std::int64_t current_offset_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_OBJECT_READ_SOURCE_H

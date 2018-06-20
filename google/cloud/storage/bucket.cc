// Copyright 2018 Google LLC
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

#include "google/cloud/storage/bucket.h"
#include "google/cloud/internal/throw_delegate.h"
#include <sstream>
#include <thread>

namespace {
bool IsRetryableStatusCode(long status_code) {
  return status_code == 429 or status_code >= 500;
}
}  // namespace

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
static_assert(std::is_copy_constructible<storage::Bucket>::value,
              "storage::Bucket must be constructible");
static_assert(std::is_copy_assignable<storage::Bucket>::value,
              "storage::Bucket must be assignable");

BucketMetadata Bucket::GetMetadataImpl(
    internal::GetBucketMetadataRequest const& request) {
  // TODO(#555) - use policies to implement retry loop.
  Status last_status;
  constexpr int MAX_NUM_RETRIES = 3;
  for (int i = 0; i != MAX_NUM_RETRIES; ++i) {
    auto result = client_->GetBucketMetadata(request);
    last_status = std::move(result.first);
    if (last_status.ok()) {
      return std::move(result.second);
    }
    // TODO(#581) - use policies to determine what error codes are permanent.
    if (not IsRetryableStatusCode(last_status.status_code())) {
      std::ostringstream os;
      os << "Permanent error in " << __func__ << ": " << last_status;
      google::cloud::internal::RaiseRuntimeError(os.str());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::ostringstream os;
  os << "Retry policy exhausted in " << __func__ << ": " << last_status;
  google::cloud::internal::RaiseRuntimeError(os.str());
}

ObjectMetadata Bucket::InsertObjectMediaImpl(
    internal::InsertObjectMediaRequest const& request) {
  // TODO(#555) - use policies to implement retry loop.
  Status last_status;
  constexpr int MAX_NUM_RETRIES = 3;
  for (int i = 0; i != MAX_NUM_RETRIES; ++i) {
    auto result = client_->InsertObjectMedia(request);
    last_status = std::move(result.first);
    if (last_status.ok()) {
      return std::move(result.second);
    }
    // TODO(#714) - use policies to decide if the operation is idempotent.
    // TODO(#581) - use policies to determine what error codes are permanent.
    if (not IsRetryableStatusCode(last_status.status_code())) {
      std::ostringstream os;
      os << "Permanent error in " << __func__ << ": " << last_status;
      google::cloud::internal::RaiseRuntimeError(os.str());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::ostringstream os;
  os << "Retry policy exhausted in " << __func__ << ": " << last_status;
  google::cloud::internal::RaiseRuntimeError(os.str());
}

void Bucket::ValidateBucketName(std::string const& bucket_name) {
  // Before creating the bucket URL let's make sure the name does not require
  // url encoding.  If it does, it is an invalid bucket according to:
  //     https://cloud.google.com/storage/docs/naming#requirements
  // anyway, and the server would reject it.
  auto pos =
      bucket_name.find_first_not_of(".-_0123456789abcdefghijklmnopqrstuvwxyz");
  if (std::string::npos != pos) {
    std::ostringstream os;
    os << "Invalid character in bucket name, only lowercase letters, numbers"
       << " and '.', '-', '_' are allowed.  First invalid char is "
       << bucket_name[pos] << ", bucket_id=" << bucket_name;
    google::cloud::internal::RaiseInvalidArgument(os.str());
  }
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

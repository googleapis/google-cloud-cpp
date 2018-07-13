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

#include "google/cloud/storage/client.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/storage/internal/curl_client.h"
#include "google/cloud/storage/internal/logging_client.h"
#include "google/cloud/storage/internal/retry_client.h"
#include <sstream>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
static_assert(std::is_copy_constructible<storage::Client>::value,
              "storage::Client must be constructible");
static_assert(std::is_copy_assignable<storage::Client>::value,
              "storage::Client must be assignable");

Client::Client(ClientOptions options)
    : Client(std::shared_ptr<internal::RawClient>(
          new internal::CurlClient(std::move(options)))) {}

BucketMetadata Client::GetBucketMetadataImpl(
    internal::GetBucketMetadataRequest const& request) {
  return raw_client_->GetBucketMetadata(request).second;
}

ObjectMetadata Client::InsertObjectMediaImpl(
    internal::InsertObjectMediaRequest const& request) {
  return raw_client_->InsertObjectMedia(request).second;
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

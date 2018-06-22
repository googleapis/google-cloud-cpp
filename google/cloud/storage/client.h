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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_H_

#include "google/cloud/storage/bucket_metadata.h"
#include "google/cloud/storage/client_options.h"
#include "google/cloud/storage/credentials.h"
#include "google/cloud/storage/internal/get_bucket_metadata_request.h"
#include "google/cloud/storage/internal/insert_object_media_request.h"
#include "google/cloud/storage/internal/read_object_range_request.h"
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/status.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Define the interface used to communicate with Google Cloud Storage.
 *
 * This is a dependency injection point so higher-level abstractions (like
 * `storage::Bucket` or `storage::Object`) can be effectively tested.
 */
class Client {
 public:
  virtual ~Client() = default;

  // The member functions of this class are not intended for general use by
  // application developers (they are simply a dependency injection point). Make
  // them protected, so the mock classes can override them, and then make the
  // classes that do use them friends.
 protected:
  friend class Bucket;
  friend class Object;
  friend class ObjectReadStreamBuf;
  /**
   * Execute a request to fetch bucket metadata.
   *
   * TODO(#690) - consider checking that modifiers in a request are compatible.
   */
  virtual std::pair<Status, BucketMetadata> GetBucketMetadata(
      internal::GetBucketMetadataRequest const& request) = 0;

  virtual std::pair<Status, ObjectMetadata> InsertObjectMedia(
      internal::InsertObjectMediaRequest const&) = 0;

  virtual std::pair<Status, internal::ReadObjectRangeResponse>
  ReadObjectRangeMedia(internal::ReadObjectRangeRequest const&) = 0;
};

/**
 * Create the default client type given the options.
 */
std::shared_ptr<Client> CreateDefaultClient(ClientOptions options);

/**
 * Create the default client type with the default configuration.
 */
inline std::shared_ptr<Client> CreateDefaultClient() {
  return CreateDefaultClient(ClientOptions());
}

/**
 * Create the default client type given the credentials.
 */
inline std::shared_ptr<Client> CreateDefaultClient(
    std::shared_ptr<Credentials> credentials) {
  return CreateDefaultClient(ClientOptions(std::move(credentials)));
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_H_

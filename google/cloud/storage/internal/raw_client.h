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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RAW_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RAW_CLIENT_H_

#include "google/cloud/storage/bucket_metadata.h"
#include "google/cloud/storage/client_options.h"
#include "google/cloud/storage/credentials.h"
#include "google/cloud/storage/internal/delete_object_request.h"
#include "google/cloud/storage/internal/empty_response.h"
#include "google/cloud/storage/internal/get_bucket_metadata_request.h"
#include "google/cloud/storage/internal/get_object_metadata_request.h"
#include "google/cloud/storage/internal/insert_object_media_request.h"
#include "google/cloud/storage/internal/list_buckets_request.h"
#include "google/cloud/storage/internal/list_object_acl_request.h"
#include "google/cloud/storage/internal/list_objects_request.h"
#include "google/cloud/storage/internal/object_acl_requests.h"
#include "google/cloud/storage/internal/object_streambuf.h"
#include "google/cloud/storage/internal/read_object_range_request.h"
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/status.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Define the interface used to communicate with Google Cloud Storage.
 *
 * This is a dependency injection point so higher-level abstractions (like
 * `storage::Bucket` or `storage::Object`) can be effectively tested.
 */
class RawClient {
 public:
  virtual ~RawClient() = default;

  virtual ClientOptions const& client_options() const = 0;

  virtual std::pair<Status, ListBucketsResponse> ListBuckets(
      ListBucketsRequest const& request) = 0;

  /**
   * Execute a request to fetch bucket metadata.
   *
   */
  virtual std::pair<Status, BucketMetadata> GetBucketMetadata(
      GetBucketMetadataRequest const& request) = 0;

  virtual std::pair<Status, ObjectMetadata> InsertObjectMedia(
      InsertObjectMediaRequest const&) = 0;

  virtual std::pair<Status, ObjectMetadata> GetObjectMetadata(
      GetObjectMetadataRequest const& request) = 0;

  virtual std::pair<Status, std::unique_ptr<ObjectReadStreambuf>> ReadObject(
      ReadObjectRangeRequest const&) = 0;
  virtual std::pair<Status, std::unique_ptr<ObjectWriteStreambuf>> WriteObject(
      InsertObjectStreamingRequest const&) = 0;

  virtual std::pair<Status, ListObjectsResponse> ListObjects(
      ListObjectsRequest const&) = 0;

  virtual std::pair<Status, EmptyResponse> DeleteObject(
      DeleteObjectRequest const&) = 0;

  virtual std::pair<Status, ListObjectAclResponse> ListObjectAcl(
      ListObjectAclRequest const&) = 0;
  virtual std::pair<Status, ObjectAccessControl> CreateObjectAcl(
      CreateObjectAclRequest const&) = 0;
  virtual std::pair<Status, EmptyResponse> DeleteObjectAcl(
      ObjectAclRequest const&) = 0;
  virtual std::pair<Status, ObjectAccessControl> GetObjectAcl(
      ObjectAclRequest const&) = 0;
  virtual std::pair<Status, ObjectAccessControl> UpdateObjectAcl(
      UpdateObjectAclRequest const&) = 0;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RAW_CLIENT_H_

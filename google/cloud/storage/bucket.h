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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_H_

#include "google/cloud/storage/bucket_metadata.h"
#include "google/cloud/storage/client.h"

namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Represent a Google Cloud Storage Bucket.
 *
 * @warning this implementation is incomplete, we are still prototyping.
 */
class Bucket {
 public:
  /// Create an object to access @p bucket_name.
  Bucket(std::shared_ptr<Client> client, std::string bucket_name)
      : client_(std::move(client)), bucket_name_(std::move(bucket_name)) {
    ValidateBucketName(bucket_name_);
  }

  /// Return the name of the bucket.
  std::string const& bucket_name() const { return bucket_name_; }

  /**
   * Fetch the bucket metadata and return it.
   *
   * @throw std::runtime_error if the metadata cannot be fetched using the
   * current policies.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc get metadata
   */
  BucketMetadata GetMetadata();

  static void ValidateBucketName(std::string const& bucket_name);

 private:
  std::shared_ptr<Client> client_;
  std::string bucket_name_;
};
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_H_

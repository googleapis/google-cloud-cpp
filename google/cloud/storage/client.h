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
#include "google/cloud/storage/credentials.h"
#include "google/cloud/storage/status.h"
#include "google/cloud/storage/well_known_parameters.h"

namespace storage {
inline namespace STORAGE_CLIENT_NS {
class GetBucketMetadataRequest {
 public:
  GetBucketMetadataRequest() = default;
  explicit GetBucketMetadataRequest(std::string bucket_name)
      : bucket_name_(std::move(bucket_name)) {}

  std::string const& bucket_name() const { return bucket_name_; }
  GetBucketMetadataRequest& set_bucket_name(std::string bucket_name) {
    bucket_name_ = std::move(bucket_name);
    return *this;
  }

  WellKnownParameters const& well_known_parameters() const {
    return well_known_parameters_;
  }

  /**
   * Apply a list of modifiers to a GetBucketMetadataRequest.
   *
   * This is a convenience function to apply multiple modifiers in a single
   * call. Note that only those modifiers accepted by the `Apply()` overloads
   * are valid.
   *
   * @tparam H the first modifier type
   * @tparam T the types of the remaining modifiers.
   * @param head the first modifier in the list.
   * @param tail the remaining modifiers in the list.
   */
  template <typename H, typename... T>
  void ApplyModifiers(H&& head, T&&... tail) {
    Apply(std::forward<H>(head));
    ApplyModifiers(std::forward<T>(tail)...);
  }

  //@{
  /// @name Apply a single modifier to the request.
  void Apply(IfMetaGenerationMatch&& p) {
    well_known_parameters_.Apply(std::move(p));
  }
  void Apply(IfMetaGenerationNotMatch&& p) {
    well_known_parameters_.Apply(std::move(p));
  }
  void Apply(Projection&& p) { well_known_parameters_.Apply(std::move(p)); }
  void Apply(UserProject&& p) { well_known_parameters_.Apply(std::move(p)); }
  //@}

  /**
   * Apply an empty list of modifiers to a GetBucketMetadataRequest.
   */
  void ApplyModifiers() {}

 private:
  std::string bucket_name_;
  WellKnownParameters well_known_parameters_;
};

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
  virtual std::pair<Status, BucketMetadata> GetBucketMetadata(
      GetBucketMetadataRequest const& request) = 0;
};

/**
 * Create the default client for the Google Cloud Storage C++ Library.
 *
 * TODO(#549) - this function will need a set of ClientOptions.
 */
std::shared_ptr<Client> CreateDefaultClient(
    std::shared_ptr<Credentials> credentials);

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_H_

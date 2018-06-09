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
#include "google/cloud/storage/object_metadata.h"
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
   * This is a shorthand to replace:
   *
   * @code
   * request.ApplyModifier(m1).ApplyModifier(m2).ApplyModifier(m3)
   * @endcode
   *
   * with:
   *
   * @code
   * request.ApplyModifiers(m1, m2, m3)
   * @endcode
   *
   * @tparam H the first modifier type
   * @tparam T the types of the remaining modifiers.
   * @param head the first modifier in the list.
   * @param tail the remaining modifiers in the list.
   */
  template <typename H, typename... T>
  GetBucketMetadataRequest& ApplyModifiers(H&& head, T&&... tail) {
    ApplyModifier(std::forward<H>(head));
    return ApplyModifiers(std::forward<T>(tail)...);
  }

  //@{
  /// @name Apply a single modifier to the request.
  GetBucketMetadataRequest& ApplyModifier(IfMetaGenerationMatch&& p) {
    well_known_parameters_.Apply(std::move(p));
    return *this;
  }
  GetBucketMetadataRequest& ApplyModifier(IfMetaGenerationNotMatch&& p) {
    well_known_parameters_.Apply(std::move(p));
    return *this;
  }
  GetBucketMetadataRequest& ApplyModifier(Projection&& p) {
    well_known_parameters_.Apply(std::move(p));
    return *this;
  }
  GetBucketMetadataRequest& ApplyModifier(UserProject&& p) {
    well_known_parameters_.Apply(std::move(p));
    return *this;
  }
  //@}

  /**
   * Apply an empty list of modifiers to a GetBucketMetadataRequest.
   */
  GetBucketMetadataRequest& ApplyModifiers() { return *this; }

 private:
  std::string bucket_name_;
  WellKnownParameters well_known_parameters_;
};

class InsertObjectMediaRequest {
 public:
  InsertObjectMediaRequest() = default;
  explicit InsertObjectMediaRequest(std::string bucket_name,
                                    std::string object_name,
                                    std::string contents)
      : bucket_name_(std::move(bucket_name)),
        object_name_(std::move(object_name)),
        contents_(std::move(contents)) {}

  std::string const& bucket_name() const { return bucket_name_; }
  InsertObjectMediaRequest& set_bucket_name(std::string bucket_name) {
    bucket_name_ = std::move(bucket_name);
    return *this;
  }
  std::string const& object_name() const { return object_name_; }
  InsertObjectMediaRequest& set_object_name(std::string object_name) {
    object_name_ = std::move(object_name);
    return *this;
  }
  std::string const& contents() const { return contents_; }
  InsertObjectMediaRequest& set_contents(std::string contents) {
    contents_ = std::move(contents);
    return *this;
  }

  /**
   * Apply a list of modifiers to a CreateObjectMediaRequest.
   *
   * This is a shorthand to replace:
   *
   * @code
   * request.ApplyModifier(m1).ApplyModifier(m2).ApplyModifier(m3)
   * @endcode
   *
   * with:
   *
   * @code
   * request.ApplyModifiers(m1, m2, m3)
   * @endcode
   *
   * @tparam H the first modifier type
   * @tparam T the types of the remaining modifiers.
   * @param head the first modifier in the list.
   * @param tail the remaining modifiers in the list.
   */
  template <typename H, typename... T>
  InsertObjectMediaRequest& ApplyModifiers(H&& head, T&&... tail) {
    ApplyModifier(std::forward<H>(head));
    return ApplyModifiers(std::forward<T>(tail)...);
  }

  //@{
  /// @name Apply a single modifier to the request.
  InsertObjectMediaRequest& ApplyModifier(IfGenerationMatch&& p) {
    well_known_parameters_.Apply(std::move(p));
    return *this;
  }
  InsertObjectMediaRequest& ApplyModifier(IfGenerationNotMatch&& p) {
    well_known_parameters_.Apply(std::move(p));
    return *this;
  }
  InsertObjectMediaRequest& ApplyModifier(IfMetaGenerationMatch&& p) {
    well_known_parameters_.Apply(std::move(p));
    return *this;
  }
  InsertObjectMediaRequest& ApplyModifier(IfMetaGenerationNotMatch&& p) {
    well_known_parameters_.Apply(std::move(p));
    return *this;
  }
  InsertObjectMediaRequest& ApplyModifier(Projection&& p) {
    well_known_parameters_.Apply(std::move(p));
    return *this;
  }
  InsertObjectMediaRequest& ApplyModifier(UserProject&& p) {
    well_known_parameters_.Apply(std::move(p));
    return *this;
  }
  //@}

  /**
   * Apply an empty list of modifiers to a InsertObjectMediaRequest.
   */
  InsertObjectMediaRequest& ApplyModifiers() { return *this; }

  WellKnownParameters const& well_known_parameters() const {
    return well_known_parameters_;
  }
 private:
  std::string bucket_name_;
  std::string object_name_;
  std::string contents_;
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
  /**
   * Execute a request to fetch bucket metadata.
   *
   * TODO(#690) - consider checking that modifiers in a request are compatible.
   */
  virtual std::pair<Status, BucketMetadata> GetBucketMetadata(
      GetBucketMetadataRequest const& request) = 0;

  // TODO(#682) - prototype modifiers for the request.
  virtual std::pair<Status, ObjectMetadata> InsertObjectMedia(
      InsertObjectMediaRequest const&) = 0;
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

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_H_

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

#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/internal/retry_client.h"
#include "google/cloud/storage/list_objects_reader.h"
#include "google/cloud/storage/object_stream.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * The Google Cloud Storage Client.
 *
 * @warning this implementation is incomplete, we are still prototyping.
 */
class Client {
 public:
  /**
   * Create the default client type given the options.
   */
  explicit Client(ClientOptions options);

  /**
   * Create the default client type with the default configuration.
   */
  explicit Client() : Client(ClientOptions()) {}

  /**
   * Create the default client type given the credentials.
   */
  explicit Client(std::shared_ptr<Credentials> credentials)
      : Client(ClientOptions(std::move(credentials))) {}

  /// Build a client with specific retry and backoff policies.
  template <typename RetryPolicy, typename BackoffPolicy>
  Client(std::shared_ptr<internal::RawClient> client, RetryPolicy retry_policy,
         BackoffPolicy backoff_policy)
      : raw_client_(new internal::RetryClient(std::move(client),
                                              std::move(retry_policy),
                                              std::move(backoff_policy))) {}

  /// Build a client with an specific RawClient, with the default retry policy
  explicit Client(std::shared_ptr<internal::RawClient> client);

  /// Build a client with an specific RawClient, without retry policies.
  struct NoRetry {};
  explicit Client(std::shared_ptr<internal::RawClient> client, NoRetry)
      : raw_client_(std::move(client)) {}

  /**
   * Fetch the bucket metadata and return it.
   *
   * @param bucket_name query metadata information about this bucket.
   * @param modifiers a variadic list. Valid types for this operation include
   *   `IfMetagenerationMatch`, `IfMetagenerationNotMatch`, `UserProject`,
   *   `Projection`.
   *
   * @throw std::runtime_error if the metadata cannot be fetched using the
   * current policies.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc get bucket metadata
   */
  template <typename... Modifiers>
  BucketMetadata GetBucketMetadata(std::string const& bucket_name,
                                   Modifiers&&... modifiers) {
    internal::GetBucketMetadataRequest request(bucket_name);
    request.set_multiple_parameters(std::forward<Modifiers>(modifiers)...);
    return GetBucketMetadataImpl(request);
  }

  /**
   * Create an object given its name and media (contents).
   *
   * @param bucket_name the name of the bucket that will contain the object.
   * @param object_name the name of the object to be created.
   * @param contents the contents (media) for the new object.
   * @param modifiers a variadic list. Valid types for this operation include
   *   `IfMetagenerationMatch`, `IfMetagenerationNotMatch`, `UserProject`,
   *   `Projection`.
   *
   * @throw std::runtime_error if the operation cannot be completed using the
   *   current policies.
   *
   * @par Example
   * @snippet storage_object_samples.cc insert object
   */
  template <typename... Modifiers>
  ObjectMetadata InsertObject(std::string const& bucket_name,
                              std::string const& object_name,
                              std::string contents, Modifiers&&... modifiers) {
    internal::InsertObjectMediaRequest request(bucket_name, object_name,
                                               std::move(contents));
    request.set_multiple_parameters(std::forward<Modifiers>(modifiers)...);
    return InsertObjectMediaImpl(request);
  }

  /**
   * List the objects in a bucket.
   *
   * @param bucket_name the name of the bucket to list.
   * @param parameters a variadic list of optional parameters. Valid types for
   *   this operation include
   *   `IfMetagenerationMatch`, `IfMetagenerationNotMatch`, `UserProject`,
   *   `Projection`, `Prefix`.
   *
   * @throw std::runtime_error if the operation cannot be completed using the
   *   current policies.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc list objects
   */
  template <typename... Parameters>
  ListObjectsReader ListObjects(std::string const& bucket_name,
                                Parameters&&... parameters) {
    return ListObjectsReader(raw_client_, bucket_name,
                             std::forward<Parameters>(parameters)...);
  }

  /**
   * Read the contents of an object.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be read.
   * @param parameters a variadic list of optional parameters. Valid types for
   *   this operation include
   *   `IfGenerationMatch`/`IfGenerationNotMatch`, `IfMetagenerationMatch`/
   *   `IfMetagenerationNotMatch`, `Generation`, and `UserProject`.
   *
   * @par Example
   * @snippet storage_object_samples.cc read object
   */
  template <typename... Parameters>
  ObjectReadStream Read(std::string const& bucket_name,
                        std::string const& object_name,
                        Parameters&&... parameters) {
    internal::ReadObjectRangeRequest request(bucket_name, object_name);
    request.set_multiple_parameters(std::forward<Parameters>(parameters)...);
    return ObjectReadStream(raw_client_, request);
  }

 private:
  BucketMetadata GetBucketMetadataImpl(
      internal::GetBucketMetadataRequest const& request);
  ObjectMetadata InsertObjectMediaImpl(
      internal::InsertObjectMediaRequest const& request);

 private:
  std::shared_ptr<internal::RawClient> raw_client_;
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_H_

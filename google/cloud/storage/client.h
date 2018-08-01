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

#include "google/cloud/storage/internal/logging_client.h"
#include "google/cloud/storage/internal/retry_client.h"
#include "google/cloud/storage/list_buckets_reader.h"
#include "google/cloud/storage/list_objects_reader.h"
#include "google/cloud/storage/object_stream.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * The Google Cloud Storage Client.
 *
 * @warning this implementation is incomplete.
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

  /// Build a client and maybe override the retry and/or backoff policies.
  template <typename... Policies>
  explicit Client(std::shared_ptr<internal::RawClient> client,
                  Policies&&... policies)
      : raw_client_(
            Decorate(std::move(client), std::forward<Policies>(policies)...)) {}

  /// Build a client with an specific RawClient, without decorations.
  struct NoDecorations {};
  explicit Client(std::shared_ptr<internal::RawClient> client, NoDecorations)
      : raw_client_(std::move(client)) {}

  /**
   * Fetch the list of buckets for a given project.
   *
   * @param project_id the project to query.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `MaxResults`, `Prefix`,
   *     `UserProject`, and `Projection`.
   *
   * @throw std::runtime_error if the operation fails.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc list buckets for project
   */
  template <typename... Options>
  ListBucketsReader ListBucketsForProject(std::string const& project_id,
                                          Options&&... options) {
    return ListBucketsReader(raw_client_, project_id,
                             std::forward<Options>(options)...);
  }

  /**
   * Fetch the list of buckets for the default project.
   *
   * The default project is configured in the `ClientOptions` used to construct
   * this object. If the application does not set the project id in the
   * `ClientOptions`, the value of the `GOOGLE_CLOUD_PROJECT` is used. If
   * neither the environment variable is set, nor a value is set explicitly by
   * the application this function raises an exception.
   *
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `MaxResults`, `Prefix`,
   *     `UserProject`, and `Projection`.
   *
   * @throw std::logic_error if the function is called without a default
   *     project id set.
   * @throw std::runtime_error if the operation fails.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc list buckets
   */
  template <typename... Options>
  ListBucketsReader ListBuckets(Options&&... options) {
    auto const& project_id = raw_client_->client_options().project_id();
    if (project_id.empty()) {
      std::string msg = "Default project id not set in ";
      msg += __func__;
      google::cloud::internal::RaiseLogicError(msg);
    }
    return ListBucketsForProject(project_id, std::forward<Options>(options)...);
  }

  /**
   * Fetch the bucket metadata and return it.
   *
   * @param bucket_name query metadata information about this bucket.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `UserProject`, and `Projection`.
   *
   * @throw std::runtime_error if the metadata cannot be fetched using the
   * current policies.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc get bucket metadata
   */
  template <typename... Options>
  BucketMetadata GetBucketMetadata(std::string const& bucket_name,
                                   Options&&... options) {
    internal::GetBucketMetadataRequest request(bucket_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return GetBucketMetadataImpl(request);
  }

  /**
   * Create an object given its name and media (contents).
   *
   * @param bucket_name the name of the bucket that will contain the object.
   * @param object_name the name of the object to be created.
   * @param contents the contents (media) for the new object.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `UserProject`, and `Projection`.
   *
   * @throw std::runtime_error if the operation cannot be completed using the
   *   current policies.
   *
   * @par Example
   * @snippet storage_object_samples.cc insert object
   */
  template <typename... Options>
  ObjectMetadata InsertObject(std::string const& bucket_name,
                              std::string const& object_name,
                              std::string contents, Options&&... options) {
    internal::InsertObjectMediaRequest request(bucket_name, object_name,
                                               std::move(contents));
    request.set_multiple_options(std::forward<Options>(options)...);
    return InsertObjectMediaImpl(request);
  }

  /**
   * Fetch the object metadata and return it.
   *
   * @param bucket_name the bucket containing the object.
   * @param object_name the object name.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`,
   *     `IfGenerationMatch`, `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `Projection`, and `UserProject`.
   *
   *
   * @throw std::runtime_error if the metadata cannot be fetched using the
   * current policies.
   *
   * @par Example
   * @snippet storage_object_samples.cc get object metadata
   */
  template <typename... Options>
  ObjectMetadata GetObjectMetadata(std::string const& bucket_name,
                                   std::string const& object_name,
                                   Options&&... options) {
    internal::GetObjectMetadataRequest request(bucket_name, object_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->GetObjectMetadata(request).second;
  }

  /**
   * List the objects in a bucket.
   *
   * @param bucket_name the name of the bucket to list.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include
   *     `IfMetagenerationMatch`, `IfMetagenerationNotMatch`, `UserProject`,
   *     `Projection`, `Prefix`.
   *
   * @throw std::runtime_error if the operation cannot be completed using the
   *   current policies.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc list objects
   */
  template <typename... Options>
  ListObjectsReader ListObjects(std::string const& bucket_name,
                                Options&&... options) {
    return ListObjectsReader(raw_client_, bucket_name,
                             std::forward<Options>(options)...);
  }

  /**
   * Read the contents of an object.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be read.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include
   *     `IfGenerationMatch`, `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `Generation`, and `UserProject`.
   *
   * @par Example
   * @snippet storage_object_samples.cc read object
   */
  template <typename... Options>
  ObjectReadStream ReadObject(std::string const& bucket_name,
                              std::string const& object_name,
                              Options&&... options) {
    internal::ReadObjectRangeRequest request(bucket_name, object_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return ObjectReadStream(raw_client_->ReadObject(request).second);
  }

  /**
   * Write contents into an object.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be read.
   * @param options a list of optional query parameters and/or request headers.
   *   Valid types for this operation include `IfGenerationMatch`,
   *   `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *   `IfMetagenerationNotMatch`, `Generation`, and `UserProject`.
   */
  template <typename... Options>
  ObjectWriteStream WriteObject(std::string const& bucket_name,
                                std::string const& object_name,
                                Options&&... options) {
    internal::InsertObjectStreamingRequest request(bucket_name, object_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return ObjectWriteStream(raw_client_->WriteObject(request).second);
  }

  /**
   * Delete an object.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be deleted.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`,
   *     `IfGenerationMatch`, `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, and `UserProject`.
   *
   * @par Example
   * @snippet storage_object_samples.cc delete object
   */
  template <typename... Options>
  void DeleteObject(std::string const& bucket_name,
                    std::string const& object_name, Options&&... options) {
    internal::DeleteObjectRequest request(bucket_name, object_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    raw_client_->DeleteObject(request);
  }

  /**
   * Retrieve the list of ObjectAccessControls for an object.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be deleted.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`, and `UserProject`.
   *
   * @par Example
   * @snippet storage_object_acl_samples.cc list object acl
   */
  template <typename... Options>
  std::vector<ObjectAccessControl> ListObjectAcl(std::string const& bucket_name,
                                                 std::string const& object_name,
                                                 Options&&... options) {
    internal::ListObjectAclRequest request(bucket_name, object_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->ListObjectAcl(request).second.items;
  }

  /**
   * Create a new entry in the object ACL.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object.
   * @param entity the name of the entity added to the ACL.
   * @param role the role of the entity.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`, and `UserProject`.
   *
   * @par Example
   * @snippet storage_object_acl_samples.cc create object acl
   */
  template <typename... Options>
  ObjectAccessControl CreateObjectAcl(std::string const& bucket_name,
                                      std::string const& object_name,
                                      std::string const& entity,
                                      std::string const& role,
                                      Options&&... options) {
    internal::CreateObjectAclRequest request(bucket_name, object_name, entity,
                                             role);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->CreateObjectAcl(request).second;
  }

  /**
   * Delete one access control entry in one object.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be deleted.
   * @param entity the name of the entity (user, team, group) to be removed from
   *   the Object's ACL.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`, and `UserProject`.
   *
   * @par Example
   * @snippet storage_object_acl_samples.cc delete object acl
   */
  template <typename... Options>
  void DeleteObjectAcl(std::string const& bucket_name,
                       std::string const& object_name,
                       std::string const& entity, Options&&... options) {
    internal::ObjectAclRequest request(bucket_name, object_name, entity);
    request.set_multiple_options(std::forward<Options>(options)...);
    raw_client_->DeleteObjectAcl(request);
  }

  /**
   * Get the value of an existing object ACL.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object.
   * @param entity the name of the entity added to the ACL.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`, and `UserProject`.
   *
   * @par Example
   * @snippet storage_object_acl_samples.cc get object acl
   */
  template <typename... Options>
  ObjectAccessControl GetObjectAcl(std::string const& bucket_name,
                                   std::string const& object_name,
                                   std::string const& entity,
                                   Options&&... options) {
    internal::ObjectAclRequest request(bucket_name, object_name, entity);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->GetObjectAcl(request).second;
  }

  /**
   * Update the value of an existing object ACL.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object.
   * @param acl the new ACL value. Note that only the writable values of the ACL
   *   will be modified by the server.
   * @param options a list of optional query parameters and/or request
   *     Valid types for this operation include `Generation`, and `UserProject`.
   *
   * @par Example
   * @snippet storage_object_acl_samples.cc update object acl
   *
   * @see https://cloud.google.com/storage/docs/json_api/v1/objectAccessControls
   *     for additional details on what fields are writeable.
   */
  template <typename... Options>
  ObjectAccessControl UpdateObjectAcl(std::string const& bucket_name,
                                      std::string const& object_name,
                                      ObjectAccessControl const& acl,
                                      Options&&... options) {
    internal::UpdateObjectAclRequest request(bucket_name, object_name,
                                             acl.entity(), acl.role());
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->UpdateObjectAcl(request).second;
  }

  std::shared_ptr<internal::RawClient> raw_client() const {
    return raw_client_;
  }

 private:
  BucketMetadata GetBucketMetadataImpl(
      internal::GetBucketMetadataRequest const& request);

  ObjectMetadata InsertObjectMediaImpl(
      internal::InsertObjectMediaRequest const& request);

  template <typename... Policies>
  std::shared_ptr<internal::RawClient> Decorate(
      std::shared_ptr<internal::RawClient> client, Policies&&... policies) {
    auto logging = std::make_shared<internal::LoggingClient>(std::move(client));
    auto retry = std::make_shared<internal::RetryClient>(
        std::move(logging), std::forward<Policies>(policies)...);
    return retry;
  }

 private:
  std::shared_ptr<internal::RawClient> raw_client_;
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_H_

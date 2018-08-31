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

  std::shared_ptr<internal::RawClient> raw_client() const {
    return raw_client_;
  }

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
   * Creates a new Google Cloud Storage Bucket.
   *
   * @param bucket_name the name of the new bucket.
   * @param metadata the metadata for the new Bucket.  The `name` field is
   *     ignored in favor of @p bucket_name.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `PredefinedAcl`,
   *     `PredefinedDefaultObjectAcl`, `Projection`, and `UserProject`.
   *
   * @throw std::runtime_error if the operation fails.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc create bucket
   */
  template <typename... Options>
  BucketMetadata CreateBucket(std::string bucket_name, BucketMetadata metadata,
                              Options&&... options) {
    auto const& project_id = raw_client_->client_options().project_id();
    if (project_id.empty()) {
      std::string msg = "Default project id not set in ";
      msg += __func__;
      google::cloud::internal::RaiseLogicError(msg);
    }
    return CreateBucketForProject(std::move(bucket_name), project_id,
                                  std::move(metadata),
                                  std::forward<Options>(options)...);
  }

  /**
   * Creates a new Google Cloud Storage Bucket in a given project.
   *
   * @param bucket_name the name of the new bucket.
   * @param project_id the id of the project that will host the new bucket.
   * @param metadata the metadata for the new Bucket.  The `name` field is
   *     ignored in favor of @p bucket_name.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `PredefinedAcl`,
   *     `PredefinedDefaultObjectAcl`, `Projection`, and `UserProject`.
   *
   * @throw std::runtime_error if the operation fails.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc create bucket for project
   */
  template <typename... Options>
  BucketMetadata CreateBucketForProject(std::string bucket_name,
                                        std::string project_id,
                                        BucketMetadata metadata,
                                        Options&&... options) {
    metadata.set_name(std::move(bucket_name));
    internal::CreateBucketRequest request(std::move(project_id),
                                          std::move(metadata));
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->CreateBucket(request).second;
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
   * Deletes a Google Cloud Storage Bucket.
   *
   * @param bucket_name the bucket to be deleted.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, and `UserProject`.
   *
   * @throw std::runtime_error if the metadata cannot be fetched using the
   * current policies.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc delete bucket
   */
  template <typename... Options>
  void DeleteBucket(std::string const& bucket_name, Options&&... options) {
    internal::DeleteBucketRequest request(bucket_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    raw_client_->DeleteBucket(request);
  }

  /**
   * Updates the metadata in a Google Cloud Storage Bucket.
   *
   * @param bucket_name the name of the new bucket.
   * @param metadata the new metadata for the Bucket.  The `name` field is
   *     ignored in favor of @p bucket_name.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `PredefinedAcl`,
   *     `PredefinedDefaultObjectAcl`, `Projection`, and `UserProject`.
   *
   * @throw std::runtime_error if the operation fails.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc update bucket
   */
  template <typename... Options>
  BucketMetadata UpdateBucket(std::string bucket_name, BucketMetadata metadata,
                              Options&&... options) {
    metadata.set_name(std::move(bucket_name));
    internal::UpdateBucketRequest request(std::move(metadata));
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->UpdateBucket(request).second;
  }

  /**
   * Patch the metadata in a Google Cloud Storage Bucket.
   *
   * This function creates a patch request to change the writeable attributes in
   * @p original to the values in @p updated.  Non-writeable attributes are
   * ignored, and attributes not present in @p updated are removed. Typically
   * this function is used after the application obtained a value with
   * `GetBucketMetadata` and has modified these parameters.
   *
   * @param bucket_name the bucket to be updated.
   * @param original the initial value of the bucket metadata.
   * @param updated the updated value for the bucket metadata.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `Projection`, and `UserProject`.
   *
   * @throw std::runtime_error if the metadata cannot be fetched using the
   * current policies.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc patch bucket storage class
   */
  template <typename... Options>
  BucketMetadata PatchBucket(std::string bucket_name,
                             BucketMetadata const& original,
                             BucketMetadata const& updated,
                             Options&&... options) {
    internal::PatchBucketRequest request(std::move(bucket_name), original,
                                         updated);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchBucket(request).second;
  }

  /**
   * Patch the metadata in a Google Cloud Storage Bucket.
   *
   * This function creates a patch request based on the given @p builder.
   *
   * @param bucket_name the bucket to be updated.
   * @param builder the set of updates to perform in the Bucket.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `Projection`, and `UserProject`.
   *
   * @throw std::runtime_error if the metadata cannot be fetched using the
   * current policies.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc patch bucket storage class with builder
   */
  template <typename... Options>
  BucketMetadata PatchBucket(std::string bucket_name,
                             BucketMetadataPatchBuilder const& builder,
                             Options&&... options) {
    internal::PatchBucketRequest request(std::move(bucket_name), builder);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchBucket(request).second;
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
   *     `Projection`, `Prefix`, and `Versions`.
   *
   * @throw std::runtime_error if the operation cannot be completed using the
   *   current policies.
   *
   * @par Example
   * @snippet storage_object_samples.cc list objects
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
   * Updates the metadata in a Google Cloud Storage Object.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object.
   * @param metadata the new metadata for the Object.  Only the writeable fields
   *     accepted by the `Objects: update` API are used, all other fields are
   *     ignored. In particular, note that `bucket` and `name` are ignored in
   *     favor of @p bucket_name and @p object_name.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`,
   *     `IfGenerationMatch`, `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `PredefinedAcl`,
   *     `Projection`, and `UserProject`.
   *
   * @throw std::runtime_error if the operation fails.
   *
   * @par Example
   * @snippet storage_object_samples.cc update object metadata
   */
  template <typename... Options>
  ObjectMetadata UpdateObject(std::string bucket_name, std::string object_name,
                              ObjectMetadata metadata, Options&&... options) {
    internal::UpdateObjectRequest request(
        std::move(bucket_name), std::string(object_name), std::move(metadata));
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->UpdateObject(request).second;
  }

  /**
   * Retrieves the list of BucketAccessControls for a bucket.
   *
   * @param bucket_name the name of the bucket.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Example
   * @snippet storage_bucket_acl_samples.cc list bucket acl
   */
  template <typename... Options>
  std::vector<BucketAccessControl> ListBucketAcl(std::string const& bucket_name,
                                                 Options&&... options) {
    internal::ListBucketAclRequest request(bucket_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->ListBucketAcl(request).second.items;
  }

  /**
   * Creates a new entry in a bucket ACL.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the name of the entity added to the ACL.
   * @param role the role of the entity.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @snippet storage_bucket_acl_samples.cc create bucket acl
   */
  template <typename... Options>
  BucketAccessControl CreateBucketAcl(std::string const& bucket_name,
                                      std::string const& entity,
                                      std::string const& role,
                                      Options&&... options) {
    internal::CreateBucketAclRequest request(bucket_name, entity, role);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->CreateBucketAcl(request).second;
  }

  /**
   * Deletes an entry from a bucket ACL.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the name of the entity added to the ACL.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @snippet storage_bucket_acl_samples.cc delete bucket acl
   */
  template <typename... Options>
  void DeleteBucketAcl(std::string const& bucket_name,
                       std::string const& entity, Options&&... options) {
    internal::DeleteBucketAclRequest request(bucket_name, entity);
    request.set_multiple_options(std::forward<Options>(options)...);
    raw_client_->DeleteBucketAcl(request);
  }

  /**
   * Gets the value of an existing bucket ACL.
   *
   * @param bucket_name the name of the bucket to query.
   * @param entity the name of the entity to query.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Example
   * @snippet storage_bucket_acl_samples.cc get bucket acl
   */
  template <typename... Options>
  BucketAccessControl GetBucketAcl(std::string const& bucket_name,
                                   std::string const& entity,
                                   Options&&... options) {
    internal::GetBucketAclRequest request(bucket_name, entity);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->GetBucketAcl(request).second;
  }

  /**
   * Update the value of an existing bucket ACL.
   *
   * @param bucket_name the name of the bucket.
   * @param acl the new ACL value. Note that only the writable values of the ACL
   *   will be modified by the server.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Example
   * @snippet storage_bucket_acl_samples.cc update bucket acl
   *
   * @see https://cloud.google.com/storage/docs/json_api/v1/bucketAccessControls
   *     for additional details on what fields are writeable.
   */
  template <typename... Options>
  BucketAccessControl UpdateBucketAcl(std::string const& bucket_name,
                                      BucketAccessControl const& acl,
                                      Options&&... options) {
    internal::UpdateBucketAclRequest request(bucket_name, acl.entity(),
                                             acl.role());
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->UpdateBucketAcl(request).second;
  }

  /**
   * Patch the value of an existing bucket ACL.
   *
   * Computes the delta between a previous value for an BucketAccessControl and
   * the new value for an BucketAccessControl and apply that delta.
   *
   * @par Notes
   * For changing BucketAccessControl the Patch and Update APIs basically offer
   * the same functionality. The only field that can be modified by either API
   * is `role`, and it may only be set to a new value (it cannot be removed).
   * The API is offered for consistency with the other resource types where
   * Patch and Update APIs have different semantics.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the identifier for the user, group, service account, or
   *     predefined set of actors holding the permission.
   * @param original_acl the original ACL value.
   * @param new_acl the new ACL value. Note that only changes on writeable
   *     fields will be accepted by the server.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`, and the standard
   *     options available to all operations.
   *
   * @par Example
   * @snippet storage_bucket_acl_samples.cc patch bucket acl
   *
   * @see https://cloud.google.com/storage/docs/json_api/v1/bucketAccessControls
   *     for additional details on what fields are writeable.
   */
  template <typename... Options>
  BucketAccessControl PatchBucketAcl(std::string const& bucket_name,
                                     std::string const& entity,
                                     BucketAccessControl const& original_acl,
                                     BucketAccessControl const& new_acl,
                                     Options&&... options) {
    internal::PatchBucketAclRequest request(bucket_name, entity, original_acl,
                                            new_acl);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchBucketAcl(request).second;
  }

  /**
   * Patch the value of an existing bucket ACL.
   *
   * This API allows the application to patch an BucketAccessControl without
   * having to read the current value.
   *
   * @par Notes
   * For changing BucketAccessControl the Patch and Update APIs basically offer
   * the same functionality. The only field that can be modified by either API
   * is `role`, and it may only be set to a new value (it cannot be removed).
   * The API is offered for consistency with the other resource types where
   * Patch and Update APIs have different semantics.
   *
   * @param bucket_name the name of the bucket that contains the bucket.
   * @param bucket_name the name of the bucket.
   * @param entity the identifier for the user, group, service account, or
   *     predefined set of actors holding the permission.
   * @param builder a builder ready to create the patch.
   * @param options a list of optional query parameters and/or request
   *     headers. Valid types for this operation include `Generation`,
   *     `UserProject`, `IfMatchEtag`, and `IfNoneMatchEtag`.
   *
   * @par Example
   * @snippet storage_bucket_acl_samples.cc patch bucket acl no-read
   *
   * @see https://cloud.google.com/storage/docs/json_api/v1/bucketAccessControls
   *     for additional details on what fields are writeable.
   */
  template <typename... Options>
  BucketAccessControl PatchBucketAcl(
      std::string const& bucket_name, std::string const& entity,
      BucketAccessControlPatchBuilder const& builder, Options&&... options) {
    internal::PatchBucketAclRequest request(bucket_name, entity, builder);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchBucketAcl(request).second;
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
    internal::DeleteObjectAclRequest request(bucket_name, object_name, entity);
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
    internal::GetObjectAclRequest request(bucket_name, object_name, entity);
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

  /**
   * Patch the value of an existing object ACL.
   *
   * Compute the delta between a previous value for an ObjectAccessControl and
   * the new value for an ObjectAccessControl and apply that delta.
   *
   * @par Notes
   * For changing ObjectAccessControl the Patch and Update APIs basically offer
   * the same functionality. The only field that can be modified by either API
   * is `role`, and it may only be set to a new value (it cannot be removed).
   * The API is offered for consistency with the other resource types where
   * Patch and Update APIs have different semantics.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object.
   * @param entity the identifier for the user, group, service account, or
   *     predefined set of actors holding the permission.
   * @param original_acl the original ACL value.
   * @param new_acl the new ACL value. Note that only changes on writeable
   *     fields will be accepted by the server.
   * @param options a list of optional query parameters and/or request
   *     headers. Valid types for this operation include `Generation`,
   *     `UserProject`, `IfMatchEtag`, and `IfNoneMatchEtag`.
   *
   * @par Example
   * @snippet storage_object_acl_samples.cc patch object acl
   *
   * @see https://cloud.google.com/storage/docs/json_api/v1/objectAccessControls
   *     for additional details on what fields are writeable.
   */
  template <typename... Options>
  ObjectAccessControl PatchObjectAcl(std::string const& bucket_name,
                                     std::string const& object_name,
                                     std::string const& entity,
                                     ObjectAccessControl const& original_acl,
                                     ObjectAccessControl const& new_acl,
                                     Options&&... options) {
    internal::PatchObjectAclRequest request(bucket_name, object_name, entity,
                                            original_acl, new_acl);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchObjectAcl(request).second;
  }

  /**
   * Patch the value of an existing object ACL.
   *
   * This API allows the application to patch an ObjectAccessControl without
   * having to read the current value.
   *
   * @par Notes
   * For changing ObjectAccessControl the Patch and Update APIs basically offer
   * the same functionality. The only field that can be modified by either API
   * is `role`, and it may only be set to a new value (it cannot be removed).
   * The API is offered for consistency with the other resource types where
   * Patch and Update APIs have different semantics.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object.
   * @param entity the identifier for the user, group, service account, or
   *     predefined set of actors holding the permission.
   * @param builder a builder ready to create the patch.
   * @param options a list of optional query parameters and/or request
   *     headers. Valid types for this operation include `Generation`,
   *     `UserProject`, `IfMatchEtag`, and `IfNoneMatchEtag`.
   *
   * @par Example
   * @snippet storage_object_acl_samples.cc patch object acl no-read
   *
   * @see https://cloud.google.com/storage/docs/json_api/v1/objectAccessControls
   *     for additional details on what fields are writeable.
   */
  template <typename... Options>
  ObjectAccessControl PatchObjectAcl(
      std::string const& bucket_name, std::string const& object_name,
      std::string const& entity, ObjectAccessControlPatchBuilder const& builder,
      Options&&... options) {
    internal::PatchObjectAclRequest request(bucket_name, object_name, entity,
                                            builder);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchObjectAcl(request).second;
  }

  /**
   * Retrieves the default object ACL for a bucket.
   *
   * The default object ACL sets the ACL for any object created in the bucket,
   * unless a different ACL is specified when the object is created.
   *
   * @param bucket_name the name of the bucket.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Example
   * @snippet storage_default_object_acl_samples.cc list default object acl
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/create-manage-lists#defaultobjects
   */
  template <typename... Options>
  std::vector<ObjectAccessControl> ListDefaultObjectAcl(
      std::string const& bucket_name, Options&&... options) {
    internal::ListDefaultObjectAclRequest request(bucket_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->ListDefaultObjectAcl(request).second.items;
  }

  /**
   * Creates a new entry in the default object ACL for a bucket.
   *
   * The default object ACL sets the ACL for any object created in the bucket,
   * unless a different ACL is specified when the object is created.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the name of the entity added to the ACL.
   * @param role the role of the entity.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @snippet storage_default_object_acl_samples.cc create default object acl
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/create-manage-lists#defaultobjects
   */
  template <typename... Options>
  ObjectAccessControl CreateDefaultObjectAcl(std::string const& bucket_name,
                                             std::string const& entity,
                                             std::string const& role,
                                             Options&&... options) {
    internal::CreateDefaultObjectAclRequest request(bucket_name, entity, role);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->CreateDefaultObjectAcl(request).second;
  }

  /**
   * Deletes an entry from the default object ACL in a bucket.
   *
   * The default object ACL sets the ACL for any object created in the bucket,
   * unless a different ACL is specified when the object is created.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the name of the entity added to the ACL.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @snippet storage_default_object_acl_samples.cc delete default object acl
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/create-manage-lists#defaultobjects
   */
  template <typename... Options>
  void DeleteDefaultObjectAcl(std::string const& bucket_name,
                              std::string const& entity, Options&&... options) {
    internal::DeleteDefaultObjectAclRequest request(bucket_name, entity);
    request.set_multiple_options(std::forward<Options>(options)...);
    raw_client_->DeleteDefaultObjectAcl(request);
  }

  /**
   * Gets the value of a default object ACL in a bucket.
   *
   * The default object ACL sets the ACL for any object created in the bucket,
   * unless a different ACL is specified when the object is created.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the name of the entity.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Example
   * @snippet storage_default_object_acl_samples.cc get default object acl
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/create-manage-lists#defaultobjects
   */
  template <typename... Options>
  ObjectAccessControl GetDefaultObjectAcl(std::string const& bucket_name,
                                          std::string const& entity,
                                          Options&&... options) {
    internal::GetDefaultObjectAclRequest request(bucket_name, entity);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->GetDefaultObjectAcl(request).second;
  }

  /**
   * Updates the value of an existing default object ACL.
   *
   * The default object ACL sets the ACL for any object created in the bucket,
   * unless a different ACL is specified when the object is created.
   *
   * @param bucket_name the name of the bucket.
   * @param acl the new ACL value. Note that only the writable values of the ACL
   *   will be modified by the server.
   * @param options a list of optional query parameters and/or request
   *     Valid types for this operation include `UserProject`.
   *
   * @par Example
   * @snippet storage_default_object_acl_samples.cc update default object acl
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/create-manage-lists#defaultobjects
   */
  template <typename... Options>
  ObjectAccessControl UpdateDefaultObjectAcl(std::string const& bucket_name,
                                             ObjectAccessControl const& acl,
                                             Options&&... options) {
    internal::UpdateDefaultObjectAclRequest request(bucket_name, acl.entity(),
                                                    acl.role());
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->UpdateDefaultObjectAcl(request).second;
  }

  /**
   * Patches the value of an existing default object ACL.
   *
   * Compute the delta between a previous and new values for a default object
   * access control, and apply that delta.
   *
   * @par Notes
   * For changing default object access controls the Patch and Update APIs
   * basically offer the same functionality. The only field that can be modified
   * by either API is `role`, and it may only be set to a new value (it cannot
   * be removed). The API is offered for consistency with the other resource
   * types where Patch and Update APIs have different semantics.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the identifier for the user, group, service account, or
   *     predefined set of actors holding the permission.
   * @param original_acl the original ACL value.
   * @param new_acl the new ACL value. Note that only changes on writeable
   *     fields will be accepted by the server.
   * @param options a list of optional query parameters and/or request
   *     headers. Valid types for this operation include `UserProject`, as well
   *     as the standard parameters, such as `IfMatchEtag`, and
   *     `IfNoneMatchEtag`.
   *
   * @par Example
   * @snippet storage_default_object_acl_samples.cc patch default object acl
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/create-manage-lists#defaultobjects
   */
  template <typename... Options>
  ObjectAccessControl PatchDefaultObjectAcl(
      std::string const& bucket_name, std::string const& entity,
      ObjectAccessControl const& original_acl,
      ObjectAccessControl const& new_acl, Options&&... options) {
    internal::PatchDefaultObjectAclRequest request(bucket_name, entity,
                                                   original_acl, new_acl);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchDefaultObjectAcl(request).second;
  }

  /**
   * Patches the value of an existing default object ACL.
   *
   * This API allows the application to patch an ObjectAccessControl without
   * having to read the current value.
   *
   * @par Notes
   * For changing default object access controls the Patch and Update APIs
   * basically offer the same functionality. The only field that can be modified
   * by either API is `role`, and it may only be set to a new value (it cannot
   * be removed). The API is offered for consistency with the other resource
   * types where Patch and Update APIs have different semantics.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the identifier for the user, group, service account, or
   *     predefined set of actors holding the permission.
   * @param builder a builder ready to create the patch.
   * @param options a list of optional query parameters and/or request
   *     headers. Valid types for this operation include `UserProject`, as well
   *     as the standard parameters, such as `IfMatchEtag`, and
   *     `IfNoneMatchEtag`.
   *
   * @par Example
   * @snippet storage_default_object_acl_samples.cc patch default object acl
   * no-read
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/create-manage-lists#defaultobjects
   */
  template <typename... Options>
  ObjectAccessControl PatchDefaultObjectAcl(
      std::string const& bucket_name, std::string const& entity,
      ObjectAccessControlPatchBuilder const& builder, Options&&... options) {
    internal::PatchDefaultObjectAclRequest request(bucket_name, entity,
                                                   builder);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchDefaultObjectAcl(request).second;
  }

  /**
   * Get the GCS service account for a given project.
   *
   * A service account is a special Google account that belongs to your
   * application, virtual machine, or to a Google service when acting on your
   * behalf.  This API allows you to discover the GCS service account for the
   * @p project_id project.
   *
   * @param project_id the project to query.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @throw std::runtime_error if the operation fails.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc get service account for project
   *
   * @see https://cloud.google.com/iam/docs/service-accounts for general
   *     information on Google Cloud Platform service accounts.
   */
  template <typename... Options>
  ServiceAccount GetServiceAccountForProject(std::string const& project_id,
                                             Options&&... options) {
    internal::GetProjectServiceAccountRequest request(project_id);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->GetServiceAccount(request).second;
  }

  /**
   * Get the GCS service account for the default project.
   *
   * A service account is a special Google account that belongs to your
   * application, virtual machine, or to a Google service when acting on your
   * behalf.  This API allows you to discover the GCS service account for the
   * default project associated with this object.
   *
   * The default project is configured in the `ClientOptions` used to construct
   * this object. If the application does not set the project id in the
   * `ClientOptions`, the value of the `GOOGLE_CLOUD_PROJECT` is used. If
   * neither the environment variable is set, nor a value is set explicitly by
   * the application this function raises an exception.
   *
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @throw std::logic_error if the function is called without a default
   *     project id set.
   * @throw std::runtime_error if the operation fails.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc get service account
   *
   * @see https://cloud.google.com/iam/docs/service-accounts for general
   *     information on Google Cloud Platform service accounts.
   */
  template <typename... Options>
  ServiceAccount GetServiceAccount(Options&&... options) {
    auto const& project_id = raw_client_->client_options().project_id();
    if (project_id.empty()) {
      std::string msg = "Default project id not set in ";
      msg += __func__;
      google::cloud::internal::RaiseLogicError(msg);
    }
    return GetServiceAccountForProject(project_id,
                                       std::forward<Options>(options)...);
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

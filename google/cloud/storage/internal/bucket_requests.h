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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_BUCKET_REQUESTS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_BUCKET_REQUESTS_H_

#include "google/cloud/iam_policy.h"
#include "google/cloud/storage/bucket_metadata.h"
#include "google/cloud/storage/internal/generic_request.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/well_known_parameters.h"
#include <iosfwd>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Requests the list of buckets for a project.
 */
class ListBucketsRequest
    : public GenericRequest<ListBucketsRequest, MaxResults, Prefix, Projection,
                            UserProject> {
 public:
  ListBucketsRequest() = default;
  explicit ListBucketsRequest(std::string project_id)
      : project_id_(std::move(project_id)) {}

  std::string const& project_id() const { return project_id_; }
  std::string const& page_token() const { return page_token_; }
  ListBucketsRequest& set_page_token(std::string page_token) {
    page_token_ = std::move(page_token);
    return *this;
  }

 private:
  std::string project_id_;
  std::string page_token_;
};

std::ostream& operator<<(std::ostream& os, ListBucketsRequest const& r);

struct ListBucketsResponse {
  static ListBucketsResponse FromHttpResponse(HttpResponse&& response);

  std::string next_page_token;
  std::vector<BucketMetadata> items;
};

std::ostream& operator<<(std::ostream& os, ListBucketsResponse const& r);

/**
 * Requests the metadata for a bucket.
 */
class GetBucketMetadataRequest
    : public GenericRequest<GetBucketMetadataRequest, IfMetagenerationMatch,
                            IfMetagenerationNotMatch, Projection, UserProject> {
 public:
  GetBucketMetadataRequest() = default;
  explicit GetBucketMetadataRequest(std::string bucket_name)
      : bucket_name_(std::move(bucket_name)) {}

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
};

std::ostream& operator<<(std::ostream& os, GetBucketMetadataRequest const& r);

/**
 * Represents a request to the `Buckets: insert` API.
 */
class CreateBucketRequest
    : public GenericRequest<CreateBucketRequest, PredefinedAcl,
                            PredefinedDefaultObjectAcl, Projection,
                            UserProject> {
 public:
  CreateBucketRequest() = default;
  explicit CreateBucketRequest(std::string project_id, BucketMetadata metadata)
      : project_id_(std::move(project_id)), metadata_(std::move(metadata)) {}

  /// Returns the request as the JSON API payload.
  std::string json_payload() const { return metadata_.ToJsonString(); }

  std::string const& project_id() const { return project_id_; }
  CreateBucketRequest& set_project_id(std::string project_id) {
    project_id_ = std::move(project_id);
    return *this;
  }

  BucketMetadata const& metadata() const { return metadata_; }
  CreateBucketRequest& set_metadata(BucketMetadata metadata) {
    metadata_ = std::move(metadata);
    return *this;
  }

 private:
  std::string project_id_;
  BucketMetadata metadata_;
};

std::ostream& operator<<(std::ostream& os, CreateBucketRequest const& r);

/**
 * Represents a request to the `Buckets: delete` API.
 */
class DeleteBucketRequest
    : public GenericRequest<DeleteBucketRequest, IfMetagenerationMatch,
                            IfMetagenerationNotMatch, UserProject> {
 public:
  DeleteBucketRequest() = default;
  explicit DeleteBucketRequest(std::string bucket_name)
      : bucket_name_(std::move(bucket_name)) {}

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
};

std::ostream& operator<<(std::ostream& os, DeleteBucketRequest const& r);

/**
 * Represents a request to the `Buckets: update` API.
 */
class UpdateBucketRequest
    : public GenericRequest<
          UpdateBucketRequest, IfMetagenerationMatch, IfMetagenerationNotMatch,
          PredefinedAcl, PredefinedDefaultObjectAcl, Projection, UserProject> {
 public:
  UpdateBucketRequest() = default;
  explicit UpdateBucketRequest(BucketMetadata metadata)
      : metadata_(std::move(metadata)) {}

  /// Returns the request as the JSON API payload.
  std::string json_payload() const { return metadata_.ToJsonString(); }

  BucketMetadata const& metadata() const { return metadata_; }

 private:
  BucketMetadata metadata_;
};

std::ostream& operator<<(std::ostream& os, UpdateBucketRequest const& r);

/**
 * Represents a request to the `Buckets: patch` API.
 */
class PatchBucketRequest
    : public GenericRequest<
          PatchBucketRequest, IfMetagenerationMatch, IfMetagenerationNotMatch,
          PredefinedAcl, PredefinedDefaultObjectAcl, Projection, UserProject> {
 public:
  PatchBucketRequest() = default;
  explicit PatchBucketRequest(std::string bucket,
                              BucketMetadata const& original,
                              BucketMetadata const& updated);
  explicit PatchBucketRequest(std::string bucket,
                              BucketMetadataPatchBuilder const& patch);

  std::string const& bucket() const { return bucket_; }
  std::string const& payload() const { return payload_; }

 private:
  std::string bucket_;
  std::string payload_;
};

std::ostream& operator<<(std::ostream& os, PatchBucketRequest const& r);

/**
 * Represents a request to the `Buckets: getIamPolicy` API.
 */
class GetBucketIamPolicyRequest
    : public GenericRequest<GetBucketIamPolicyRequest, UserProject> {
 public:
  GetBucketIamPolicyRequest() = default;
  explicit GetBucketIamPolicyRequest(std::string bucket_name)
      : bucket_name_(std::move(bucket_name)) {}

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
};

std::ostream& operator<<(std::ostream& os, GetBucketIamPolicyRequest const& r);

IamPolicy ParseIamPolicyFromString(std::string const& payload);

/**
 * Represents a request to the `Buckets: getIamPolicy` API.
 */
class SetBucketIamPolicyRequest
    : public GenericRequest<SetBucketIamPolicyRequest, UserProject> {
 public:
  SetBucketIamPolicyRequest() = default;
  explicit SetBucketIamPolicyRequest(std::string bucket_name,
                                     google::cloud::IamPolicy const& policy);

  std::string const& bucket_name() const { return bucket_name_; }
  std::string const& json_payload() const { return json_payload_; }

 private:
  std::string bucket_name_;
  std::string json_payload_;
};

std::ostream& operator<<(std::ostream& os, SetBucketIamPolicyRequest const& r);

/**
 * Represents a request to the `Buckets: testIamPolicyPermissions` API.
 */
class TestBucketIamPermissionsRequest
    : public GenericRequest<TestBucketIamPermissionsRequest, UserProject> {
 public:
  TestBucketIamPermissionsRequest() = default;
  explicit TestBucketIamPermissionsRequest(std::string bucket_name,
                                           std::vector<std::string> permissions)
      : bucket_name_(std::move(bucket_name)),
        permissions_(std::move(permissions)) {}

  std::string const& bucket_name() const { return bucket_name_; }
  std::vector<std::string> const& permissions() const { return permissions_; }

 private:
  std::string bucket_name_;
  std::vector<std::string> permissions_;
};

std::ostream& operator<<(std::ostream& os,
                         TestBucketIamPermissionsRequest const& r);

struct TestBucketIamPermissionsResponse {
  static TestBucketIamPermissionsResponse FromHttpResponse(
      HttpResponse const& response);

  std::vector<std::string> permissions;
};

std::ostream& operator<<(std::ostream& os,
                         TestBucketIamPermissionsResponse const& r);

/**
 * Represents a request to the `Buckets: lockRetentionPolicy` API.
 */
class LockBucketRetentionPolicyRequest
    : public GenericRequest<LockBucketRetentionPolicyRequest, UserProject> {
 public:
  LockBucketRetentionPolicyRequest() = default;
  explicit LockBucketRetentionPolicyRequest(std::string bucket_name,
                                            std::uint64_t metageneration)
      : bucket_name_(std::move(bucket_name)), metageneration_(metageneration) {}

  std::string const& bucket_name() const { return bucket_name_; }
  std::uint64_t metageneration() const { return metageneration_; }

 private:
  std::string bucket_name_;
  std::uint64_t metageneration_;
};

std::ostream& operator<<(std::ostream& os,
                         LockBucketRetentionPolicyRequest const& r);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_BUCKET_REQUESTS_H_

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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_BUCKET_ACL_REQUESTS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_BUCKET_ACL_REQUESTS_H_

#include "google/cloud/storage/bucket_access_control.h"
#include "google/cloud/storage/internal/generic_request.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/well_known_parameters.h"
#include <iosfwd>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
struct BucketAccessControlParser {
  static StatusOr<BucketAccessControl> FromJson(internal::nl::json const& json);

  static StatusOr<BucketAccessControl> FromString(std::string const& payload);
};

/// Represents a request to call the `BucketAccessControl: list` API.
class ListBucketAclRequest
    : public GenericRequest<ListBucketAclRequest, UserProject> {
 public:
  ListBucketAclRequest() : GenericRequest() {}
  explicit ListBucketAclRequest(std::string bucket)
      : bucket_name_(std::move(bucket)) {}

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
};

std::ostream& operator<<(std::ostream& os, ListBucketAclRequest const& r);

/// Represents a response to the `BucketAccessControl: list` API.
struct ListBucketAclResponse {
  static StatusOr<ListBucketAclResponse> FromHttpResponse(
      HttpResponse&& response);

  std::vector<BucketAccessControl> items;
};

std::ostream& operator<<(std::ostream& os, ListBucketAclResponse const& r);

/**
 * Represents common attributes to multiple `BucketAccessControls` request
 * types.
 *
 * The classes to represent requests for the `BucketAccessControls: create`,
 * `get`, `delete`, `patch`, and `update` APIs have a lot of commonality. This
 * template class refactors that code.
 */
template <typename Derived>
class GenericBucketAclRequest : public GenericRequest<Derived, UserProject> {
 public:
  GenericBucketAclRequest() = default;
  GenericBucketAclRequest(std::string bucket, std::string entity)
      : bucket_name_(std::move(bucket)), entity_(std::move(entity)) {}

  std::string const& bucket_name() const { return bucket_name_; }
  std::string const& entity() const { return entity_; }

 private:
  std::string bucket_name_;
  std::string entity_;
};

/**
 * Represents a request to call the `BucketAccessControls: get` API.
 */
class GetBucketAclRequest
    : public GenericBucketAclRequest<GetBucketAclRequest> {
  using GenericBucketAclRequest::GenericBucketAclRequest;
};

std::ostream& operator<<(std::ostream& os, GetBucketAclRequest const& r);

/**
 * Represents a request to call the `BucketAccessControls: delete` API.
 */
class DeleteBucketAclRequest
    : public GenericBucketAclRequest<DeleteBucketAclRequest> {
  using GenericBucketAclRequest::GenericBucketAclRequest;
};

std::ostream& operator<<(std::ostream& os, DeleteBucketAclRequest const& r);

/**
 * Represents common attributes to multiple `BucketAccessControls` request
 * types.
 *
 * The classes that represent requests for the `BucketAccessControls: create`,
 * `patch`, and `update` APIs have a lot of commonality. This
 * template class refactors that code.
 */
template <typename Derived>
class GenericChangeBucketAclRequest : public GenericBucketAclRequest<Derived> {
 public:
  GenericChangeBucketAclRequest() = default;

  explicit GenericChangeBucketAclRequest(std::string bucket, std::string entity,
                                         std::string role)
      : GenericBucketAclRequest<Derived>(std::move(bucket), std::move(entity)),
        role_(std::move(role)) {}

  std::string const& role() const { return role_; }

 private:
  std::string role_;
};

/**
 * Represents a request to call the `BucketAccessControls: insert` API.
 */
class CreateBucketAclRequest
    : public GenericChangeBucketAclRequest<CreateBucketAclRequest> {
 public:
  using GenericChangeBucketAclRequest::GenericChangeBucketAclRequest;
};

std::ostream& operator<<(std::ostream& os, CreateBucketAclRequest const& r);

/**
 * Represents a request to call the `BucketAccessControls: update` API.
 */
class UpdateBucketAclRequest
    : public GenericChangeBucketAclRequest<UpdateBucketAclRequest> {
 public:
  using GenericChangeBucketAclRequest::GenericChangeBucketAclRequest;
};

std::ostream& operator<<(std::ostream& os, UpdateBucketAclRequest const& r);

/**
 * Represents a request to call the `BucketAccessControls: patch` API.
 */
class PatchBucketAclRequest
    : public GenericBucketAclRequest<PatchBucketAclRequest> {
 public:
  PatchBucketAclRequest(std::string bucket, std::string entity,
                        BucketAccessControl const& original,
                        BucketAccessControl const& new_acl);
  PatchBucketAclRequest(std::string bucket, std::string entity,
                        BucketAccessControlPatchBuilder const& patch);

  std::string const& payload() const { return payload_; }

 private:
  std::string payload_;
};

std::ostream& operator<<(std::ostream& os, PatchBucketAclRequest const& r);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_BUCKET_ACL_REQUESTS_H_

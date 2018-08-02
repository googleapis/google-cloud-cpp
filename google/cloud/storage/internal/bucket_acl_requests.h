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
/// Represents a request to call the `BucketAccessControl: list` API.
class ListBucketAclRequest
    : public GenericRequest<ListBucketAclRequest, Generation, UserProject> {
 public:
  ListBucketAclRequest() : GenericRequest() {}
  explicit ListBucketAclRequest(std::string bucket)
      : bucket_name_(std::move(bucket)) {}

  std::string const& bucket_name() const { return bucket_name_; }
  ListBucketAclRequest& set_bucket_name(std::string v) {
    bucket_name_ = std::move(v);
    return *this;
  }

 private:
  std::string bucket_name_;
};

std::ostream& operator<<(std::ostream& os, ListBucketAclRequest const& r);

/// Represents a response to the `BucketAccessControl: list` API.
struct ListBucketAclResponse {
  static ListBucketAclResponse FromHttpResponse(HttpResponse&& response);

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
class GenericBucketAclRequest
    : public GenericRequest<Derived, Generation, UserProject> {
 public:
  GenericBucketAclRequest() = default;
  GenericBucketAclRequest(std::string bucket, std::string entity)
      : bucket_name_(std::move(bucket)), entity_(std::move(entity)) {}

  std::string const& bucket_name() const { return bucket_name_; }
  Derived& set_bucket_name(std::string v) {
    bucket_name_ = std::move(v);
    return *static_cast<Derived*>(this);
  }

  std::string const& entity() const { return entity_; }
  Derived& set_entity(std::string v) {
    entity_ = std::move(v);
    return *static_cast<Derived*>(this);
  }

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
 * Represents a request to call the `BucketAccessControls: insert` API.
 */
class CreateBucketAclRequest
    : public GenericBucketAclRequest<CreateBucketAclRequest> {
 public:
  CreateBucketAclRequest() = default;

  explicit CreateBucketAclRequest(std::string bucket, std::string entity,
                                  std::string role)
      : GenericBucketAclRequest<CreateBucketAclRequest>(std::move(bucket),
                                                        std::move(entity)),
        role_(std::move(role)) {}

  std::string const& role() const { return role_; }
  CreateBucketAclRequest& set_role(std::string v) {
    role_ = std::move(v);
    return *this;
  }

 private:
  std::string role_;
};

std::ostream& operator<<(std::ostream& os, CreateBucketAclRequest const& r);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_BUCKET_ACL_REQUESTS_H_

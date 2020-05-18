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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_DEFAULT_OBJECT_ACL_REQUESTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_DEFAULT_OBJECT_ACL_REQUESTS_H

#include "google/cloud/storage/internal/generic_request.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/object_access_control.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/storage/well_known_headers.h"
#include "google/cloud/storage/well_known_parameters.h"
#include <iosfwd>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

/// Represents a request to call the `DefaultObjectAccessControls: list` API.
class ListDefaultObjectAclRequest
    : public GenericRequest<ListDefaultObjectAclRequest, UserProject> {
 public:
  ListDefaultObjectAclRequest() = default;
  explicit ListDefaultObjectAclRequest(std::string bucket)
      : bucket_name_(std::move(bucket)) {}

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
};

std::ostream& operator<<(std::ostream& os,
                         ListDefaultObjectAclRequest const& r);

/// Represents a response to the `DefaultObjectAccessControls: list` API.
struct ListDefaultObjectAclResponse {
  static StatusOr<ListDefaultObjectAclResponse> FromHttpResponse(
      std::string const& payload);

  std::vector<ObjectAccessControl> items;
};

std::ostream& operator<<(std::ostream& os,
                         ListDefaultObjectAclResponse const& r);

/**
 * Represents common attributes to multiple `DefaultObjectAccessControls`
 * request types.
 *
 * The classes to represent requests for the `DefaultObjectAccessControls: get`,
 * `create`, `delete`, `patch`, and `update` APIs have a lot of commonality.
 * This template class refactors that code.
 */
template <typename Derived>
class GenericDefaultObjectAclRequest
    : public GenericRequest<Derived, UserProject> {
 public:
  GenericDefaultObjectAclRequest() = default;
  GenericDefaultObjectAclRequest(std::string bucket, std::string entity)
      : bucket_name_(std::move(bucket)), entity_(std::move(entity)) {}

  std::string const& bucket_name() const { return bucket_name_; }
  std::string const& entity() const { return entity_; }

 private:
  std::string bucket_name_;
  std::string entity_;
};

/**
 * Represents a request to call the `DefaultObjectAccessControls: get` API.
 */
class GetDefaultObjectAclRequest
    : public GenericDefaultObjectAclRequest<GetDefaultObjectAclRequest> {
  using GenericDefaultObjectAclRequest::GenericDefaultObjectAclRequest;
};

std::ostream& operator<<(std::ostream& os, GetDefaultObjectAclRequest const& r);

/**
 * Represents a request to call the `DefaultObjectAccessControls: delete` API.
 */
class DeleteDefaultObjectAclRequest
    : public GenericDefaultObjectAclRequest<DeleteDefaultObjectAclRequest> {
  using GenericDefaultObjectAclRequest::GenericDefaultObjectAclRequest;
};

std::ostream& operator<<(std::ostream& os,
                         DeleteDefaultObjectAclRequest const& r);

/**
 * Represents common attributes to multiple `DefaultObjectAccessControls`
 * request types.
 *
 * The classes that represent requests for the
 * `DefaultObjectAccessControls: create`, `patch`, and `update` APIs have a lot
 * of commonality. This template class refactors that code.
 */
template <typename Derived>
class GenericChangeDefaultObjectAclRequest
    : public GenericDefaultObjectAclRequest<Derived> {
 public:
  GenericChangeDefaultObjectAclRequest() = default;

  explicit GenericChangeDefaultObjectAclRequest(
      // NOLINTNEXTLINE(performance-unnecessary-value-param) TODO(#4112)
      std::string bucket, std::string entity, std::string role)
      : GenericDefaultObjectAclRequest<Derived>(std::move(bucket),
                                                std::move(entity)),
        role_(std::move(role)) {}

  std::string const& role() const { return role_; }

 private:
  std::string role_;
};

/**
 * Represents a request to call the `DefaultObjectAccessControls: insert` API.
 */
class CreateDefaultObjectAclRequest
    : public GenericChangeDefaultObjectAclRequest<
          CreateDefaultObjectAclRequest> {
 public:
  using GenericChangeDefaultObjectAclRequest::
      GenericChangeDefaultObjectAclRequest;
};

std::ostream& operator<<(std::ostream& os,
                         CreateDefaultObjectAclRequest const& r);

/**
 * Represents a request to call the `DefaultObjectAccessControls: update` API.
 */
class UpdateDefaultObjectAclRequest
    : public GenericChangeDefaultObjectAclRequest<
          UpdateDefaultObjectAclRequest> {
 public:
  using GenericChangeDefaultObjectAclRequest::
      GenericChangeDefaultObjectAclRequest;
};

std::ostream& operator<<(std::ostream& os,
                         UpdateDefaultObjectAclRequest const& r);

/**
 * Represents a request to call the `DefaultObjectAccessControls: patch` API.
 */
class PatchDefaultObjectAclRequest
    : public GenericDefaultObjectAclRequest<PatchDefaultObjectAclRequest> {
 public:
  PatchDefaultObjectAclRequest(std::string bucket, std::string entity,
                               ObjectAccessControl const& original,
                               ObjectAccessControl const& new_acl);
  PatchDefaultObjectAclRequest(std::string bucket, std::string entity,
                               ObjectAccessControlPatchBuilder const& patch);

  std::string const& payload() const { return payload_; }

 private:
  std::string payload_;
};

std::ostream& operator<<(std::ostream& os,
                         PatchDefaultObjectAclRequest const& r);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_DEFAULT_OBJECT_ACL_REQUESTS_H

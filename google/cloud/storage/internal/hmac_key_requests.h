// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HMAC_KEY_REQUESTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HMAC_KEY_REQUESTS_H

#include "google/cloud/storage/hmac_key_metadata.h"
#include "google/cloud/storage/internal/generic_request.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/override_default_project.h"
#include "google/cloud/storage/version.h"
#include <iosfwd>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

template <typename Derived, typename... Options>
class GenericHmacKeyRequest : public GenericRequest<Derived, Options...> {
 public:
  GenericHmacKeyRequest() = default;
  explicit GenericHmacKeyRequest(std::string project_id)
      : project_id_(std::move(project_id)) {}

  std::string const& project_id() const { return project_id_; }

  //@{
  /**
   * @name Handle request options.
   *
   * Modify the default implementation from GenericRequest. We want to set the
   * `project_id_` member variable when the option is `OverrideDefaultProject`.
   */
  template <typename H, typename... T>
  Derived& set_multiple_options(H&& h, T&&... tail) {
    set_option(std::forward<H>(h));
    return set_multiple_options(std::forward<T>(tail)...);
  }

  Derived& set_multiple_options() { return *static_cast<Derived*>(this); }

  using GenericRequest<Derived, Options...>::set_option;
  void set_option(OverrideDefaultProject const& o) {
    if (o.has_value()) {
      project_id_ = o.value();
    }
  }

 private:
  std::string project_id_;
};

/// Represents a request to create a call the `HmacKeys: insert` API.
class CreateHmacKeyRequest
    : public GenericHmacKeyRequest<CreateHmacKeyRequest> {
 public:
  CreateHmacKeyRequest() = default;
  CreateHmacKeyRequest(std::string project_id, std::string service_account)
      : GenericHmacKeyRequest(std::move(project_id)),
        service_account_(std::move(service_account)) {}

  std::string const& service_account() const { return service_account_; }

 private:
  std::string service_account_;
};

std::ostream& operator<<(std::ostream& os, CreateHmacKeyRequest const& r);

/// The response from a `HmacKeys: insert` API.
struct CreateHmacKeyResponse {
  static StatusOr<CreateHmacKeyResponse> FromHttpResponse(
      std::string const& payload);

  std::string kind;
  HmacKeyMetadata metadata;
  std::string secret;
};

std::ostream& operator<<(std::ostream& os, CreateHmacKeyResponse const& r);

/// Represents a request to call the `HmacKeys: list` API.
class ListHmacKeysRequest
    : public GenericHmacKeyRequest<ListHmacKeysRequest, Deleted, MaxResults,
                                   ServiceAccountFilter, UserProject> {
 public:
  explicit ListHmacKeysRequest(std::string project_id)
      : GenericHmacKeyRequest(std::move(project_id)) {}

  std::string const& page_token() const { return page_token_; }
  ListHmacKeysRequest& set_page_token(std::string page_token) {
    page_token_ = std::move(page_token);
    return *this;
  }

 private:
  std::string page_token_;
};

std::ostream& operator<<(std::ostream& os, ListHmacKeysRequest const& r);

/// Represents a response to the `HmacKeys: list` API.
struct ListHmacKeysResponse {
  static StatusOr<ListHmacKeysResponse> FromHttpResponse(
      std::string const& payload);

  std::string next_page_token;
  std::vector<HmacKeyMetadata> items;
};

std::ostream& operator<<(std::ostream& os, ListHmacKeysResponse const& r);

/// Represents a request to call the `HmacKeys: delete` API.
class DeleteHmacKeyRequest
    : public GenericHmacKeyRequest<DeleteHmacKeyRequest> {
 public:
  explicit DeleteHmacKeyRequest(std::string project_id, std::string access_id)
      : GenericHmacKeyRequest(std::move(project_id)),
        access_id_(std::move(access_id)) {}

  std::string const& access_id() const { return access_id_; }

 private:
  std::string access_id_;
};

std::ostream& operator<<(std::ostream& os, DeleteHmacKeyRequest const& r);

/// Represents a request to call the `HmacKeys: get` API.
class GetHmacKeyRequest : public GenericHmacKeyRequest<GetHmacKeyRequest> {
 public:
  explicit GetHmacKeyRequest(std::string project_id, std::string access_id)
      : GenericHmacKeyRequest(std::move(project_id)),
        access_id_(std::move(access_id)) {}

  std::string const& access_id() const { return access_id_; }

 private:
  std::string access_id_;
};

std::ostream& operator<<(std::ostream& os, GetHmacKeyRequest const& r);

/// Represents a request to call the `HmacKeys: update` API.
class UpdateHmacKeyRequest
    : public GenericHmacKeyRequest<UpdateHmacKeyRequest> {
 public:
  explicit UpdateHmacKeyRequest(std::string project_id, std::string access_id,
                                HmacKeyMetadata resource)
      : GenericHmacKeyRequest(std::move(project_id)),
        access_id_(std::move(access_id)),
        resource_(std::move(resource)) {}

  std::string const& access_id() const { return access_id_; }
  HmacKeyMetadata const& resource() const { return resource_; }

 private:
  std::string access_id_;
  HmacKeyMetadata resource_;
};

std::ostream& operator<<(std::ostream& os, UpdateHmacKeyRequest const& r);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HMAC_KEY_REQUESTS_H

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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_HMAC_KEY_METADATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_HMAC_KEY_METADATA_H

#include "google/cloud/storage/version.h"
#include "google/cloud/status_or.h"
#include <chrono>
#include <iosfwd>
#include <string>
#include <tuple>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
struct HmacKeyMetadataParser;
}  // namespace internal

/**
 * Represents the metadata for a Google Cloud Storage HmacKeyResource.
 *
 * HMAC keys allow applications to authenticate with Google Cloud Storage using
 * HMAC authentication. Applications can create a limited number of HMAC keys
 * associated with a service account. The application can use the HMAC keys to
 * authenticate with GCS. GCS will use the service account permissions to
 * determine if the request is authorized.
 *
 * @see https://cloud.google.com/storage/docs/authentication/hmackeys for
 *     general information on HMAC keys.
 *
 * @see https://cloud.google.com/storage/ for general information on Google
 *     Cloud Storage.
 */
class HmacKeyMetadata {
 public:
  HmacKeyMetadata() = default;

  std::string const& access_id() const { return access_id_; }
  std::string const& etag() const { return etag_; }
  HmacKeyMetadata& set_etag(std::string v) {
    etag_ = std::move(v);
    return *this;
  }

  std::string const& id() const { return id_; }
  std::string const& kind() const { return kind_; }

  std::string const& project_id() const { return project_id_; }
  std::string const& service_account_email() const {
    return service_account_email_;
  }
  std::string const& state() const { return state_; }
  HmacKeyMetadata& set_state(std::string v) {
    state_ = std::move(v);
    return *this;
  }
  std::chrono::system_clock::time_point time_created() const {
    return time_created_;
  }
  std::chrono::system_clock::time_point updated() const { return updated_; }

  static std::string state_active() { return "ACTIVE"; }
  static std::string state_inactive() { return "INACTIVE"; }
  static std::string state_deleted() { return "DELETED"; }

 private:
  friend struct internal::HmacKeyMetadataParser;
  friend std::ostream& operator<<(std::ostream& os, HmacKeyMetadata const& rhs);

  // Keep the fields in alphabetical order.
  std::string access_id_;
  std::string etag_;
  std::string id_;
  std::string kind_;
  std::string project_id_;
  std::string service_account_email_;
  std::string state_;
  std::chrono::system_clock::time_point time_created_;
  std::chrono::system_clock::time_point updated_;
};

inline bool operator==(HmacKeyMetadata const& lhs, HmacKeyMetadata const& rhs) {
  auto lhs_time_created = lhs.time_created();
  auto rhs_time_created = rhs.time_created();
  auto lhs_updated = lhs.updated();
  auto rhs_updated = rhs.updated();
  return std::tie(lhs.id(), lhs.access_id(), lhs.etag(), lhs.kind(),
                  lhs.project_id(), lhs.service_account_email(), lhs.state(),
                  lhs_time_created, lhs_updated) ==
         std::tie(rhs.id(), rhs.access_id(), rhs.etag(), rhs.kind(),
                  rhs.project_id(), rhs.service_account_email(), rhs.state(),
                  rhs_time_created, rhs_updated);
}

inline bool operator!=(HmacKeyMetadata const& lhs, HmacKeyMetadata const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

std::ostream& operator<<(std::ostream& os, HmacKeyMetadata const& rhs);

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_HMAC_KEY_METADATA_H

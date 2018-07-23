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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_METADATA_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_METADATA_H_

#include "google/cloud/internal/optional.h"
#include "google/cloud/storage/bucket_access_control.h"
#include "google/cloud/storage/internal/common_metadata.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/object_access_control.h"
#include <map>
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * The billing configuration for a Bucket.
 *
 * @see https://cloud.google.com/storage/docs/requester-pays for general
 *     information on "Requester Pays" billing.
 */
struct BucketBilling {
  BucketBilling() : requester_pays(false) {}

  bool requester_pays;
};

/**
 * An entry in the CORS list.
 *
 * CORS (Cross-Origin Resource Sharing) is a mechanism to enable client-side
 * cross-origin requests. An entry in the configuration has a maximum age and a
 * list of allowed origin and methods, as well as a list of returned response
 * headers.
 *
 * @see https://en.wikipedia.org/wiki/Cross-origin_resource_sharing for general
 *     information on CORS.
 *
 * @see https://cloud.google.com/storage/docs/cross-origin for general
 *     information about CORS in the context of Google Cloud Storage.
 *
 * @see https://cloud.google.com/storage/docs/configuring-cors for information
 *     on how to set and troubleshoot CORS settings.
 */
struct CorsEntry {
  google::cloud::internal::optional<std::int64_t> max_age_seconds;
  std::vector<std::string> method;
  std::vector<std::string> origin;
  std::vector<std::string> response_header;
};

//@{
/// @name Comparison operators for CorsEntry.
inline bool operator==(CorsEntry const& lhs, CorsEntry const& rhs) {
  return std::tie(lhs.max_age_seconds, lhs.method, lhs.origin,
                  lhs.response_header) == std::tie(rhs.max_age_seconds,
                                                   rhs.method, rhs.origin,
                                                   rhs.response_header);
}

inline bool operator<(CorsEntry const& lhs, CorsEntry const& rhs) {
  return std::tie(lhs.max_age_seconds, lhs.method, lhs.origin,
                  lhs.response_header) < std::tie(rhs.max_age_seconds,
                                                  rhs.method, rhs.origin,
                                                  rhs.response_header);
}

inline bool operator!=(CorsEntry const& lhs, CorsEntry const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(CorsEntry const& lhs, CorsEntry const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(CorsEntry const& lhs, CorsEntry const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(CorsEntry const& lhs, CorsEntry const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}
//@}

std::ostream& operator<<(std::ostream& os, CorsEntry const& rhs);

/*
 * The Logging configuration for a Bucket.
 *
 * @see https://cloud.google.com/storage/docs/access-logs for general
 *     information about using access logs with Google Cloud Storage.
 */
struct BucketLogging {
  std::string log_bucket;
  std::string log_prefix;
};

inline bool operator==(BucketLogging const& lhs, BucketLogging const& rhs) {
  return std::tie(lhs.log_bucket, lhs.log_prefix) ==
         std::tie(rhs.log_bucket, rhs.log_prefix);
}

inline bool operator<(BucketLogging const& lhs, BucketLogging const& rhs) {
  return std::tie(lhs.log_bucket, lhs.log_prefix) <
         std::tie(rhs.log_bucket, rhs.log_prefix);
}

inline bool operator!=(BucketLogging const& lhs, BucketLogging const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(BucketLogging const& lhs, BucketLogging const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(BucketLogging const& lhs, BucketLogging const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(BucketLogging const& lhs, BucketLogging const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

std::ostream& operator<<(std::ostream& os, BucketLogging const& rhs);

/**
 * A simple wrapper for the encryption field in `storage#bucket`.
 */
struct BucketEncryption {
  std::string default_kms_key_name;
};

/**
 * The versioning configuration for a Bucket.
 *
 * @see https://cloud.google.com/storage/docs/requester-pays for general
 *     information on "Requester Pays" billing.
 */
struct BucketVersioning {
  BucketVersioning() : enabled(true) {}
  explicit BucketVersioning(bool flag) : enabled(flag) {}

  bool enabled;
};

inline bool operator==(BucketVersioning const& lhs,
                       BucketVersioning const& rhs) {
  return lhs.enabled == rhs.enabled;
}

inline bool operator<(BucketVersioning const& lhs,
                      BucketVersioning const& rhs) {
  return lhs.enabled < rhs.enabled;
}

inline bool operator!=(BucketVersioning const& lhs,
                       BucketVersioning const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(BucketVersioning const& lhs,
                      BucketVersioning const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(BucketVersioning const& lhs,
                       BucketVersioning const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(BucketVersioning const& lhs,
                       BucketVersioning const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

/**
 * The website configuration for a Bucket.
 *
 * @see https://cloud.google.com/storage/docs/static-website for information on
 *     how to configure Buckets to serve as a static website.
 */
struct BucketWebsite {
  std::string main_page_suffix;
  std::string not_found_page;
};

inline bool operator==(BucketWebsite const& lhs, BucketWebsite const& rhs) {
  return std::tie(lhs.main_page_suffix, lhs.not_found_page) ==
         std::tie(rhs.main_page_suffix, rhs.not_found_page);
}

inline bool operator<(BucketWebsite const& lhs, BucketWebsite const& rhs) {
  return std::tie(lhs.main_page_suffix, lhs.not_found_page) <
         std::tie(rhs.main_page_suffix, rhs.not_found_page);
}

inline bool operator!=(BucketWebsite const& lhs, BucketWebsite const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(BucketWebsite const& lhs, BucketWebsite const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(BucketWebsite const& lhs, BucketWebsite const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(BucketWebsite const& lhs, BucketWebsite const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

/**
 * Represents a Google Cloud Storage Bucket Metadata object.
 *
 * @warning This is an incomplete implementation to validate the design. It does
 * not support changes to the metadata. It also lacks support for ACLs, CORS,
 * encryption keys, lifecycle rules, and other features.
 *
 * TODO(#537) - complete the implementation.
 */
class BucketMetadata : private internal::CommonMetadata<BucketMetadata> {
 public:
  BucketMetadata() : project_number_(0) {}

  static BucketMetadata ParseFromJson(internal::nl::json const& json);
  static BucketMetadata ParseFromString(std::string const& payload);

  // Please keep these in alphabetical order, that make it easier to verify we
  // have actually implemented all of them.
  //@{
  /**
   * @name Get and set Bucket Access Control Lists.
   *
   * @see https://cloud.google.com/storage/docs/access-control/lists
   */
  std::vector<BucketAccessControl> const& acl() const { return acl_; }
  std::vector<BucketAccessControl>& mutable_acl() { return acl_; }
  BucketMetadata& set_acl(std::vector<BucketAccessControl> acl) {
    acl_ = std::move(acl);
    return *this;
  }
  //@}

  //@{
  /**
   * @name Get and set billing configuration for the Bucket.
   *
   * @see https://cloud.google.com/storage/docs/requester-pays
   */
  BucketBilling const& billing() const { return billing_; }
  BucketMetadata& set_billing(BucketBilling const& v) {
    billing_ = v;
    return *this;
  }
  //@}

  //@{
  /**
   * @name Get and set CORS configuration for the Bucket.
   *
   * @see https://en.wikipedia.org/wiki/Cross-origin_resource_sharing for
   *     general information on CORS.
   *
   * @see https://cloud.google.com/storage/docs/cross-origin for general
   *     information about CORS in the context of Google Cloud Storage.
   *
   * @see https://cloud.google.com/storage/docs/configuring-cors for information
   *     on how to set and troubleshoot CORS settings.
   */
  std::vector<CorsEntry> const& cors() const { return cors_; }
  std::vector<CorsEntry>& mutable_cors() { return cors_; }
  BucketMetadata& set_cors(std::vector<CorsEntry> cors) {
    cors_ = std::move(cors);
    return *this;
  }
  //@}

  //@{
  /**
   * @name Get and set the Default Object Access Control Lists.
   *
   * @see https://cloud.google.com/storage/docs/access-control/lists#default for
   *     general information of default ACLs.
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/create-manage-lists#defaultobjects
   *     for information on how to set the default ACLs.
   */
  std::vector<ObjectAccessControl> const& default_acl() const {
    return default_acl_;
  }
  std::vector<ObjectAccessControl>& mutable_default_acl() {
    return default_acl_;
  }
  BucketMetadata& set_default_acl(std::vector<ObjectAccessControl> acl) {
    default_acl_ = std::move(acl);
    return *this;
  }
  //@}

  //@{
  /**
   * @name Get and set the Default Object Access Control Lists.
   *
   * @see https://cloud.google.com/storage/docs/access-control/lists#default for
   *     general information of default ACLs.
   *
   * @see
   * https://cloud.google.com/storage/docs/encryption/customer-managed-keys
   *     for information on Customer-Managed Encryption Keys.
   */
  BucketEncryption const& encryption() const { return encryption_; }
  BucketMetadata& set_encryption(BucketEncryption v) {
    encryption_ = std::move(v);
    return *this;
  }
  //@}

  using CommonMetadata::etag;
  using CommonMetadata::id;
  using CommonMetadata::kind;

  std::size_t label_count() const { return labels_.size(); }
  bool has_label(std::string const& key) const {
    return labels_.end() != labels_.find(key);
  }
  std::string const& label(std::string const& key) const {
    return labels_.at(key);
  }
  std::string const& location() const { return location_; }

  BucketLogging const& logging() const { return logging_; }
  BucketMetadata& set_logging(BucketLogging v) {
    logging_ = v;
    return *this;
  }

  using CommonMetadata::metageneration;
  using CommonMetadata::name;
  using CommonMetadata::owner;

  std::int64_t const& project_number() const { return project_number_; }

  using CommonMetadata::self_link;
  using CommonMetadata::storage_class;
  using CommonMetadata::time_created;
  using CommonMetadata::updated;

  google::cloud::internal::optional<BucketVersioning> const& versioning()
      const {
    return versioning_;
  }
  BucketMetadata& enable_versioning() {
    versioning_.emplace(BucketVersioning{true});
    return *this;
  }
  BucketMetadata& disable_versioning() {
    versioning_.emplace(BucketVersioning{false});
    return *this;
  }
  BucketMetadata& clear_versioning() {
    versioning_.reset();
    return *this;
  }
  BucketMetadata& set_versioning(
      google::cloud::internal::optional<BucketVersioning> v) {
    versioning_ = std::move(v);
    return *this;
  }

  BucketWebsite const& website() const { return website_; }
  BucketMetadata& set_website(BucketWebsite v) {
    website_ = std::move(v);
    return *this;
  }

  bool operator==(BucketMetadata const& rhs) const;
  bool operator!=(BucketMetadata const& rhs) const { return not(*this == rhs); }

 private:
  friend std::ostream& operator<<(std::ostream& os, BucketMetadata const& rhs);
  // Keep the fields in alphabetical order.
  std::vector<BucketAccessControl> acl_;
  BucketBilling billing_;
  std::vector<CorsEntry> cors_;
  std::vector<ObjectAccessControl> default_acl_;
  BucketEncryption encryption_;
  std::map<std::string, std::string> labels_;
  std::string location_;
  BucketLogging logging_;
  std::int64_t project_number_;
  google::cloud::internal::optional<BucketVersioning> versioning_;
  BucketWebsite website_;
};

std::ostream& operator<<(std::ostream& os, BucketMetadata const& rhs);

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_METADATA_H_

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

#include "google/cloud/optional.h"
#include "google/cloud/storage/bucket_access_control.h"
#include "google/cloud/storage/internal/common_metadata.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/internal/patch_builder.h"
#include "google/cloud/storage/lifecycle_rule.h"
#include "google/cloud/storage/object_access_control.h"
#include "google/cloud/storage/version.h"
#include <map>
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
struct BucketMetadataParser;
}  // namespace internal

/**
 * The billing configuration for a Bucket.
 *
 * @see https://cloud.google.com/storage/docs/requester-pays for general
 *     information on "Requester Pays" billing.
 */
struct BucketBilling {
  BucketBilling() : requester_pays(false) {}
  BucketBilling(bool v) : requester_pays(v) {}

  bool requester_pays;
};

inline bool operator==(BucketBilling const& lhs, BucketBilling const& rhs) {
  return lhs.requester_pays == rhs.requester_pays;
}

inline bool operator<(BucketBilling const& lhs, BucketBilling const& rhs) {
  return lhs.requester_pays < rhs.requester_pays;
}

inline bool operator!=(BucketBilling const& lhs, BucketBilling const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(BucketBilling const& lhs, BucketBilling const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(BucketBilling const& lhs, BucketBilling const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(BucketBilling const& lhs, BucketBilling const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

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
  google::cloud::optional<std::int64_t> max_age_seconds;
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

/**
 * Configure if only the IAM policies are used for access control.
 *
 * @warning this is a Beta feature of Google Cloud Storage, it is not subject
 *     to the deprecation policy and subject to change without notice.
 *
 * @see Before enabling Bucket Policy Only please review the
 *     [feature documentation][bpo-link], as well as
 *     ["Should you use Bucket Policy Only?"][bpo-should-link].
 *
 * [bpo-link]: https://cloud.google.com/storage/docs/bucket-policy-only
 * [bpo-should-link]:
 * https://cloud.google.com/storage/docs/bucket-policy-only#should-you-use
 */
struct BucketPolicyOnly {
  bool enabled;
  std::chrono::system_clock::time_point locked_time;
};

//@{
/// @name Comparison operators For BucketOnlyPolicy.
inline bool operator==(BucketPolicyOnly const& lhs,
                       BucketPolicyOnly const& rhs) {
  return std::tie(lhs.enabled, lhs.locked_time) ==
         std::tie(rhs.enabled, rhs.locked_time);
}

inline bool operator<(BucketPolicyOnly const& lhs,
                      BucketPolicyOnly const& rhs) {
  return std::tie(lhs.enabled, lhs.locked_time) <
         std::tie(rhs.enabled, rhs.locked_time);
}

inline bool operator!=(BucketPolicyOnly const& lhs,
                       BucketPolicyOnly const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(BucketPolicyOnly const& lhs,
                      BucketPolicyOnly const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(BucketPolicyOnly const& lhs,
                       BucketPolicyOnly const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(BucketPolicyOnly const& lhs,
                       BucketPolicyOnly const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}
//@}

std::ostream& operator<<(std::ostream& os, BucketPolicyOnly const& rhs);

/**
 * The IAM configuration for a Bucket.
 *
 * Currently this only holds the BucketOnlyPolicy. In the future, we may define
 * additional IAM which would be included in this object.
 *
 * @warning this is a Beta feature of Google Cloud Storage, it is not subject
 *     to the deprecation policy and subject to change without notice.
 *
 * @see Before enabling Bucket Policy Only please review the
 *     [feature documentation][bpo-link], as well as
 *     ["Should you use Bucket Policy Only?"][bpo-should-link].
 *
 * [bpo-link]: https://cloud.google.com/storage/docs/bucket-policy-only
 * [bpo-should-link]:
 * https://cloud.google.com/storage/docs/bucket-policy-only#should-you-use
 */
struct BucketIamConfiguration {
  google::cloud::optional<BucketPolicyOnly> bucket_policy_only;
};

//@{
/// @name Comparison operators for BucketIamConfiguration.
inline bool operator==(BucketIamConfiguration const& lhs,
                       BucketIamConfiguration const& rhs) {
  return lhs.bucket_policy_only == rhs.bucket_policy_only;
}

inline bool operator<(BucketIamConfiguration const& lhs,
                      BucketIamConfiguration const& rhs) {
  return lhs.bucket_policy_only < rhs.bucket_policy_only;
}

inline bool operator!=(BucketIamConfiguration const& lhs,
                       BucketIamConfiguration const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(BucketIamConfiguration const& lhs,
                      BucketIamConfiguration const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(BucketIamConfiguration const& lhs,
                       BucketIamConfiguration const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(BucketIamConfiguration const& lhs,
                       BucketIamConfiguration const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}
//@}

std::ostream& operator<<(std::ostream& os, BucketIamConfiguration const& rhs);

/**
 * The Object Lifecycle configuration for a Bucket.
 *
 * @see https://cloud.google.com/storage/docs/managing-lifecycles for general
 *     information on object lifecycle rules.
 */
struct BucketLifecycle {
  std::vector<LifecycleRule> rule;
};

//@{
/// @name Comparison operators for BucketLifecycle.
inline bool operator==(BucketLifecycle const& lhs, BucketLifecycle const& rhs) {
  return lhs.rule == rhs.rule;
}

inline bool operator<(BucketLifecycle const& lhs, BucketLifecycle const& rhs) {
  return lhs.rule < rhs.rule;
}

inline bool operator!=(BucketLifecycle const& lhs, BucketLifecycle const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(BucketLifecycle const& lhs, BucketLifecycle const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(BucketLifecycle const& lhs, BucketLifecycle const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(BucketLifecycle const& lhs, BucketLifecycle const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}
//@}

/**
 * The Logging configuration for a Bucket.
 *
 * @see https://cloud.google.com/storage/docs/access-logs for general
 *     information about using access logs with Google Cloud Storage.
 */
struct BucketLogging {
  std::string log_bucket;
  std::string log_object_prefix;
};

inline bool operator==(BucketLogging const& lhs, BucketLogging const& rhs) {
  return std::tie(lhs.log_bucket, lhs.log_object_prefix) ==
         std::tie(rhs.log_bucket, rhs.log_object_prefix);
}

inline bool operator<(BucketLogging const& lhs, BucketLogging const& rhs) {
  return std::tie(lhs.log_bucket, lhs.log_object_prefix) <
         std::tie(rhs.log_bucket, rhs.log_object_prefix);
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
 * Describes the default customer managed encryption key for a bucket.
 *
 * Customer managed encryption keys (CMEK) are encryption keys selected by the
 * user and generated by Google Cloud Key Management Service.
 *
 * @see https://cloud.google.com/storage/docs/encryption/customer-managed-keys
 *     for a general description of CMEK in Google Cloud Storage.
 *
 * @see https://cloud.google.com/kms/ for details about the Cloud Key Management
 *     Service.
 */
struct BucketEncryption {
  std::string default_kms_key_name;
};

inline bool operator==(BucketEncryption const& lhs,
                       BucketEncryption const& rhs) {
  return lhs.default_kms_key_name == rhs.default_kms_key_name;
}

inline bool operator<(BucketEncryption const& lhs,
                      BucketEncryption const& rhs) {
  return lhs.default_kms_key_name < rhs.default_kms_key_name;
}

inline bool operator!=(BucketEncryption const& lhs,
                       BucketEncryption const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(BucketEncryption const& lhs,
                      BucketEncryption const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(BucketEncryption const& lhs,
                       BucketEncryption const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(BucketEncryption const& lhs,
                       BucketEncryption const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

/**
 * The retention policy for a bucket.
 *
 * The Bucket Lock feature of Google Cloud Storage allows you to configure a
 * data retention policy for a Cloud Storage bucket. This policy governs how
 * long objects in the bucket must be retained. The feature also allows you to
 * lock the data retention policy, permanently preventing the policy from from
 * being reduced or removed.
 *
 * @see https://cloud.google.com/storage/docs/bucket-lock for a general
 *     overview
 */
struct BucketRetentionPolicy {
  std::chrono::seconds retention_period;
  std::chrono::system_clock::time_point effective_time;
  bool is_locked;
};

inline bool operator==(BucketRetentionPolicy const& lhs,
                       BucketRetentionPolicy const& rhs) {
  return std::tie(lhs.retention_period, lhs.effective_time, lhs.is_locked) ==
         std::tie(rhs.retention_period, rhs.effective_time, rhs.is_locked);
}

inline bool operator<(BucketRetentionPolicy const& lhs,
                      BucketRetentionPolicy const& rhs) {
  return std::tie(lhs.retention_period, lhs.effective_time, lhs.is_locked) <
         std::tie(rhs.retention_period, rhs.effective_time, rhs.is_locked);
}

inline bool operator!=(BucketRetentionPolicy const& lhs,
                       BucketRetentionPolicy const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(BucketRetentionPolicy const& lhs,
                      BucketRetentionPolicy const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(BucketRetentionPolicy const& lhs,
                       BucketRetentionPolicy const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(BucketRetentionPolicy const& lhs,
                       BucketRetentionPolicy const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

std::ostream& operator<<(std::ostream& os, BucketRetentionPolicy const& rhs);

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
 */
class BucketMetadata : private internal::CommonMetadata<BucketMetadata> {
 public:
  BucketMetadata() : project_number_(0) {}

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
  bool has_billing() const { return billing_.has_value(); }
  BucketBilling const& billing() const { return *billing_; }
  google::cloud::optional<BucketBilling> const& billing_as_optional() const {
    return billing_;
  }
  BucketMetadata& set_billing(BucketBilling const& v) {
    billing_ = v;
    return *this;
  }
  BucketMetadata& reset_billing() {
    billing_.reset();
    return *this;
  }
  //@}

  //@{
  /**
   * @name Get and set the default event based hold for the Bucket.
   *
   * Objects may have an event-based hold associated with them. If a Bucket
   * has the `default_event_based_hold()` parameter set, and you create a new
   * object in the bucket without specifying its event-event based hold then the
   * object gets the value set in the bucket.
   *
   * @see https://cloud.google.com/storage/docs/bucket-lock for generation
   *     information on retention policies.  The section on
   *     [Object
   * holds](https://cloud.google.com/storage/docs/bucket-lock#object-holds) is
   * particularly relevant.
   *
   * @see https://cloud.google.com/storage/docs/holding-objects for examples
   *    of using default event-based hold policy.
   */
  bool default_event_based_hold() const { return default_event_based_hold_; }
  BucketMetadata& set_default_event_based_hold(bool v) {
    default_event_based_hold_ = v;
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
  bool has_encryption() const { return encryption_.has_value(); }
  BucketEncryption const& encryption() const { return *encryption_; }
  google::cloud::optional<BucketEncryption> const& encryption_as_optional()
      const {
    return encryption_;
  }
  BucketMetadata& set_encryption(BucketEncryption v) {
    encryption_ = std::move(v);
    return *this;
  }
  BucketMetadata& reset_encryption() {
    encryption_.reset();
    return *this;
  }
  //@}

  using CommonMetadata::etag;

  //@{
  /**
   * @name Get and set the IAM configuration.
   *
   * @warning this is a Beta feature of Google Cloud Storage, it is not
   *     subject to the deprecation policy and subject to change without notice.
   *
   * @see Before enabling Bucket Policy Only please review the
   *     [feature documentation][bpo-link], as well as
   *     ["Should you use Bucket Policy Only?"][bpo-should-link].
   *
   * [bpo-link]: https://cloud.google.com/storage/docs/bucket-policy-only
   * [bpo-should-link]:
   * https://cloud.google.com/storage/docs/bucket-policy-only#should-you-use
   */
  bool has_iam_configuration() const { return iam_configuration_.has_value(); }
  BucketIamConfiguration const& iam_configuration() const {
    return *iam_configuration_;
  }
  google::cloud::optional<BucketIamConfiguration> const&
  iam_configuration_as_optional() const {
    return iam_configuration_;
  }
  BucketMetadata& set_iam_configuration(BucketIamConfiguration v) {
    iam_configuration_ = std::move(v);
    return *this;
  }
  BucketMetadata& reset_iam_configuration() {
    iam_configuration_.reset();
    return *this;
  }
  //@}

  using CommonMetadata::id;
  using CommonMetadata::kind;

  //@{
  /// @name Accessors and modifiers to the `labels`.
  bool has_label(std::string const& key) const {
    return labels_.end() != labels_.find(key);
  }
  std::string const& label(std::string const& key) const {
    return labels_.at(key);
  }
  /// Delete a label. This is a no-op if the key does not exist.
  BucketMetadata& delete_label(std::string const& key) {
    auto i = labels_.find(key);
    if (i == labels_.end()) {
      return *this;
    }
    labels_.erase(i);
    return *this;
  }

  /// Insert or update the label entry.
  BucketMetadata& upsert_label(std::string key, std::string value) {
    auto i = labels_.lower_bound(key);
    if (i == labels_.end() || i->first != key) {
      labels_.emplace_hint(i, std::move(key), std::move(value));
    } else {
      i->second = std::move(value);
    }
    return *this;
  }

  std::map<std::string, std::string> const& labels() const { return labels_; }
  std::map<std::string, std::string>& mutable_labels() { return labels_; }
  //@}

  //@{
  /**
   * @name Accessors and modifiers for object lifecycle rules.
   *
   * @see https://cloud.google.com/storage/docs/managing-lifecycles for general
   *     information on object lifecycle rules.
   */
  bool has_lifecycle() const { return lifecycle_.has_value(); }
  BucketLifecycle const& lifecycle() const { return *lifecycle_; }
  google::cloud::optional<BucketLifecycle> const& lifecycle_as_optional()
      const {
    return lifecycle_;
  }
  BucketMetadata& set_lifecycle(BucketLifecycle v) {
    lifecycle_ = std::move(v);
    return *this;
  }
  BucketMetadata& reset_lifecycle() {
    lifecycle_.reset();
    return *this;
  }
  //@}

  std::string const& location() const { return location_; }
  BucketMetadata& set_location(std::string v) {
    location_ = std::move(v);
    return *this;
  }

  std::string const& location_type() const { return location_type_; }

  //@{
  /// @name Accessors and modifiers for logging configuration.
  bool has_logging() const { return logging_.has_value(); }
  BucketLogging const& logging() const { return *logging_; }
  google::cloud::optional<BucketLogging> const& logging_as_optional() const {
    return logging_;
  }
  BucketMetadata& set_logging(BucketLogging v) {
    logging_ = v;
    return *this;
  }
  BucketMetadata& reset_logging() {
    logging_.reset();
    return *this;
  }
  //@}

  using CommonMetadata::metageneration;
  using CommonMetadata::name;
  BucketMetadata& set_name(std::string v) {
    CommonMetadata::set_name(std::move(v));
    return *this;
  }

  using CommonMetadata::has_owner;
  using CommonMetadata::owner;

  std::int64_t const& project_number() const { return project_number_; }

  using CommonMetadata::self_link;

  //@{
  /// @name Accessors and modifiers for retention policy configuration.
  bool has_retention_policy() const { return retention_policy_.has_value(); }
  BucketRetentionPolicy const& retention_policy() const {
    return *retention_policy_;
  }
  google::cloud::optional<BucketRetentionPolicy> const&
  retention_policy_as_optional() const {
    return retention_policy_;
  }
  BucketMetadata& set_retention_policy(BucketRetentionPolicy v) {
    retention_policy_ = std::move(v);
    return *this;
  }

  /**
   * Sets the retention period.
   *
   * The retention period is the only writable attribute in a retention policy.
   * This function makes it easier to set the retention policy when the
   * `BucketMetadata` object is used to update or patch the bucket.
   */
  BucketMetadata& set_retention_policy(std::chrono::seconds retention_period) {
    return set_retention_policy(BucketRetentionPolicy{retention_period});
  }

  BucketMetadata& reset_retention_policy() {
    retention_policy_.reset();
    return *this;
  }
  //@}

  using CommonMetadata::storage_class;
  BucketMetadata& set_storage_class(std::string v) {
    CommonMetadata::set_storage_class(std::move(v));
    return *this;
  }

  using CommonMetadata::time_created;
  using CommonMetadata::updated;

  //@{
  /// @name Accessors and modifiers for versioning configuration.
  google::cloud::optional<BucketVersioning> const& versioning() const {
    return versioning_;
  }
  bool has_versioning() const { return versioning_.has_value(); }
  BucketMetadata& enable_versioning() {
    versioning_.emplace(BucketVersioning{true});
    return *this;
  }
  BucketMetadata& disable_versioning() {
    versioning_.emplace(BucketVersioning{false});
    return *this;
  }
  BucketMetadata& reset_versioning() {
    versioning_.reset();
    return *this;
  }
  BucketMetadata& set_versioning(google::cloud::optional<BucketVersioning> v) {
    versioning_ = std::move(v);
    return *this;
  }
  //@}

  //@{
  /// @name Accessors and modifiers for website configuration.
  bool has_website() const { return website_.has_value(); }
  BucketWebsite const& website() const { return *website_; }
  google::cloud::optional<BucketWebsite> const& website_as_optional() const {
    return website_;
  }
  BucketMetadata& set_website(BucketWebsite v) {
    website_ = std::move(v);
    return *this;
  }
  BucketMetadata& reset_website() {
    website_.reset();
    return *this;
  }
  //@}

  friend bool operator==(BucketMetadata const& lhs, BucketMetadata const& rhs);
  friend bool operator!=(BucketMetadata const& lhs, BucketMetadata const& rhs) {
    return !(lhs == rhs);
  }

 private:
  friend struct internal::BucketMetadataParser;

  friend std::ostream& operator<<(std::ostream& os, BucketMetadata const& rhs);
  // Keep the fields in alphabetical order.
  std::vector<BucketAccessControl> acl_;
  google::cloud::optional<BucketBilling> billing_;
  std::vector<CorsEntry> cors_;
  bool default_event_based_hold_ = false;
  std::vector<ObjectAccessControl> default_acl_;
  google::cloud::optional<BucketEncryption> encryption_;
  google::cloud::optional<BucketIamConfiguration> iam_configuration_;
  std::map<std::string, std::string> labels_;
  google::cloud::optional<BucketLifecycle> lifecycle_;
  std::string location_;
  std::string location_type_;
  google::cloud::optional<BucketLogging> logging_;
  std::int64_t project_number_;
  google::cloud::optional<BucketRetentionPolicy> retention_policy_;
  google::cloud::optional<BucketVersioning> versioning_;
  google::cloud::optional<BucketWebsite> website_;
};

std::ostream& operator<<(std::ostream& os, BucketMetadata const& rhs);

/**
 * Prepares a patch for the Bucket resource.
 *
 * The Bucket resource has many modifiable fields. The application may send a
 * patch request to change (or delete) a small fraction of these fields by using
 * this object.
 *
 * @see
 * https://cloud.google.com/storage/docs/json_api/v1/how-tos/performance#patch
 *     for general information on PATCH requests for the Google Cloud Storage
 *     JSON API.
 */
class BucketMetadataPatchBuilder {
 public:
  BucketMetadataPatchBuilder() : labels_subpatch_dirty_(false) {}

  std::string BuildPatch() const;

  BucketMetadataPatchBuilder& SetAcl(std::vector<BucketAccessControl> const& v);

  /**
   * Clears the ACL for the Bucket.
   *
   * @warning Currently the server ignores requests to reset the full ACL.
   */
  BucketMetadataPatchBuilder& ResetAcl();

  BucketMetadataPatchBuilder& SetBilling(BucketBilling const& v);
  BucketMetadataPatchBuilder& ResetBilling();

  BucketMetadataPatchBuilder& SetCors(std::vector<CorsEntry> const& v);
  BucketMetadataPatchBuilder& ResetCors();

  BucketMetadataPatchBuilder& SetDefaultEventBasedHold(bool v);
  BucketMetadataPatchBuilder& ResetDefaultEventBasedHold();

  BucketMetadataPatchBuilder& SetDefaultAcl(
      std::vector<ObjectAccessControl> const& v);

  /**
   * Clears the default object ACL for the Bucket.
   *
   * @warning Currently the server ignores requests to reset the full ACL.
   */
  BucketMetadataPatchBuilder& ResetDefaultAcl();

  BucketMetadataPatchBuilder& SetIamConfiguration(
      BucketIamConfiguration const& v);
  BucketMetadataPatchBuilder& ResetIamConfiguration();

  BucketMetadataPatchBuilder& SetEncryption(BucketEncryption const& v);
  BucketMetadataPatchBuilder& ResetEncryption();

  BucketMetadataPatchBuilder& SetLabel(std::string const& label,
                                       std::string const& value);
  BucketMetadataPatchBuilder& ResetLabel(std::string const& label);
  BucketMetadataPatchBuilder& ResetLabels();

  BucketMetadataPatchBuilder& SetLifecycle(BucketLifecycle const& v);
  BucketMetadataPatchBuilder& ResetLifecycle();

  BucketMetadataPatchBuilder& SetLogging(BucketLogging const& v);
  BucketMetadataPatchBuilder& ResetLogging();

  BucketMetadataPatchBuilder& SetName(std::string const& v);
  BucketMetadataPatchBuilder& ResetName();

  BucketMetadataPatchBuilder& SetRetentionPolicy(
      BucketRetentionPolicy const& v);
  BucketMetadataPatchBuilder& SetRetentionPolicy(
      std::chrono::seconds retention_period) {
    // This is the only parameter that the application can set, so make it easy
    // for them to set it.
    return SetRetentionPolicy(BucketRetentionPolicy{retention_period});
  }
  BucketMetadataPatchBuilder& ResetRetentionPolicy();

  BucketMetadataPatchBuilder& SetStorageClass(std::string const& v);
  BucketMetadataPatchBuilder& ResetStorageClass();

  BucketMetadataPatchBuilder& SetVersioning(BucketVersioning const& v);
  BucketMetadataPatchBuilder& ResetVersioning();

  BucketMetadataPatchBuilder& SetWebsite(BucketWebsite const& v);
  BucketMetadataPatchBuilder& ResetWebsite();

 private:
  internal::PatchBuilder impl_;
  bool labels_subpatch_dirty_;
  internal::PatchBuilder labels_subpatch_;
};
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_METADATA_H_

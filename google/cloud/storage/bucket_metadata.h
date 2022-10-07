// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_METADATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_METADATA_H

#include "google/cloud/storage/bucket_access_control.h"
#include "google/cloud/storage/bucket_billing.h"
#include "google/cloud/storage/bucket_cors_entry.h"
#include "google/cloud/storage/bucket_custom_placement_config.h"
#include "google/cloud/storage/bucket_encryption.h"
#include "google/cloud/storage/bucket_iam_configuration.h"
#include "google/cloud/storage/bucket_lifecycle.h"
#include "google/cloud/storage/bucket_logging.h"
#include "google/cloud/storage/bucket_retention_policy.h"
#include "google/cloud/storage/bucket_rpo.h"
#include "google/cloud/storage/bucket_versioning.h"
#include "google/cloud/storage/bucket_website.h"
#include "google/cloud/storage/internal/patch_builder.h"
#include "google/cloud/storage/lifecycle_rule.h"
#include "google/cloud/storage/object_access_control.h"
#include "google/cloud/storage/owner.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/optional.h"
#include "absl/types/optional.h"
#include <chrono>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Represents a Google Cloud Storage Bucket Metadata object.
 */
class BucketMetadata {
 public:
  BucketMetadata() = default;

  // Please keep these in alphabetical order, that make it easier to verify we
  // have actually implemented all of them.
  /**
   * @name Get and set Bucket Access Control Lists.
   *
   * @see https://cloud.google.com/storage/docs/access-control/lists
   */
  ///@{
  std::vector<BucketAccessControl> const& acl() const { return acl_; }
  std::vector<BucketAccessControl>& mutable_acl() { return acl_; }
  BucketMetadata& set_acl(std::vector<BucketAccessControl> acl) {
    acl_ = std::move(acl);
    return *this;
  }
  ///@}

  /**
   * @name Get and set billing configuration for the Bucket.
   *
   * @see https://cloud.google.com/storage/docs/requester-pays
   */
  ///@{
  bool has_billing() const { return billing_.has_value(); }
  BucketBilling const& billing() const { return *billing_; }
  absl::optional<BucketBilling> const& billing_as_optional() const {
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
  ///@}

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
  ///@{
  bool default_event_based_hold() const { return default_event_based_hold_; }
  BucketMetadata& set_default_event_based_hold(bool v) {
    default_event_based_hold_ = v;
    return *this;
  }
  ///@}

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
  ///@{
  std::vector<CorsEntry> const& cors() const { return cors_; }
  std::vector<CorsEntry>& mutable_cors() { return cors_; }
  BucketMetadata& set_cors(std::vector<CorsEntry> cors) {
    cors_ = std::move(cors);
    return *this;
  }
  ///@}

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
  ///@{
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
  ///@}

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
  ///@{
  bool has_encryption() const { return encryption_.has_value(); }
  BucketEncryption const& encryption() const { return *encryption_; }
  absl::optional<BucketEncryption> const& encryption_as_optional() const {
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
  ///@}

  std::string const& etag() const { return etag_; }

  /// @note This is only intended for mocking.
  BucketMetadata& set_etag(std::string v) {
    etag_ = std::move(v);
    return *this;
  }

  /**
   * @name Get and set the IAM configuration.
   *
   * @see Before enabling Uniform Bucket Level Access please review the
   *     [feature documentation][ubla-link], as well as
   *     ["Should you use uniform bucket-level access ?"][ubla-should-link].
   *
   * [ubla-link]:
   * https://cloud.google.com/storage/docs/uniform-bucket-level-access
   * [ubla-should-link]:
   * https://cloud.google.com/storage/docs/uniform-bucket-level-access#should-you-use
   */
  ///@{
  bool has_iam_configuration() const { return iam_configuration_.has_value(); }
  BucketIamConfiguration const& iam_configuration() const {
    return *iam_configuration_;
  }
  absl::optional<BucketIamConfiguration> const& iam_configuration_as_optional()
      const {
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
  ///@}

  /// Return the bucket id.
  std::string const& id() const { return id_; }

  /// @note This is only intended for mocking.
  BucketMetadata& set_id(std::string v) {
    id_ = std::move(v);
    return *this;
  }

  std::string const& kind() const { return kind_; }

  /// @note This is only intended for mocking
  BucketMetadata& set_kind(std::string v) {
    kind_ = std::move(v);
    return *this;
  }

  /// @name Accessors and modifiers to the `labels`.
  ///@{

  /// Returns `true` if the key is present in the Bucket's metadata labels.
  bool has_label(std::string const& key) const {
    return labels_.end() != labels_.find(key);
  }

  /**
   * Returns the value of @p key in the Bucket's metadata labels.
   *
   * It is undefined behavior to call `label(key)` if `has_label(key) == false`.
   */
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

  /// Returns all the Bucket's labels.
  std::map<std::string, std::string> const& labels() const { return labels_; }

  /// Returns all the Bucket's labels.
  std::map<std::string, std::string>& mutable_labels() { return labels_; }
  ///@}

  /**
   * @name Accessors and modifiers for object lifecycle rules.
   *
   * @see https://cloud.google.com/storage/docs/managing-lifecycles for general
   *     information on object lifecycle rules.
   */
  ///@{
  bool has_lifecycle() const { return lifecycle_.has_value(); }
  BucketLifecycle const& lifecycle() const { return *lifecycle_; }
  absl::optional<BucketLifecycle> const& lifecycle_as_optional() const {
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
  ///@}

  /// Return the bucket location.
  std::string const& location() const { return location_; }

  /// Set the bucket location. Only applicable when creating buckets.
  BucketMetadata& set_location(std::string v) {
    location_ = std::move(v);
    return *this;
  }

  /// Returns the location type (e.g. regional vs. dual region).
  std::string const& location_type() const { return location_type_; }

  /// @note This is only intended for mocking.
  BucketMetadata& set_location_type(std::string v) {
    location_type_ = std::move(v);
    return *this;
  }

  /// @name Accessors and modifiers for logging configuration.
  ///@{
  bool has_logging() const { return logging_.has_value(); }
  BucketLogging const& logging() const { return *logging_; }
  absl::optional<BucketLogging> const& logging_as_optional() const {
    return logging_;
  }
  BucketMetadata& set_logging(BucketLogging v) {
    logging_ = std::move(v);
    return *this;
  }
  BucketMetadata& reset_logging() {
    logging_.reset();
    return *this;
  }
  ///@}

  /// The bucket metageneration.
  std::int64_t metageneration() const { return metageneration_; }

  /// @note this is only intended for mocking.
  BucketMetadata& set_metageneration(std::int64_t v) {
    metageneration_ = v;
    return *this;
  }

  /// The bucket name.
  std::string const& name() const { return name_; }

  /**
   * Changes the name.
   *
   * @note Bucket names can only be set during bucket creation. This modifier
   *   is used to set the name when using a `BucketMetadata` object that changes
   *   some other attribute.
   */
  BucketMetadata& set_name(std::string v) {
    name_ = std::move(v);
    return *this;
  }

  /// Returns true if the bucket `owner` attribute is present.
  bool has_owner() const { return owner_.has_value(); }
  /**
   * Returns the owner.
   *
   * It is undefined behavior to call `owner()` if `has_owner()` is false.
   */
  Owner const& owner() const { return *owner_; }

  /// @note this is only intended for mocking.
  BucketMetadata& set_owner(Owner v) {
    owner_ = std::move(v);
    return *this;
  }
  /// @note this is only intended for mocking.
  BucketMetadata& reset_owner() {
    owner_.reset();
    return *this;
  }

  std::int64_t const& project_number() const { return project_number_; }

  /// @note this is only intended for mocking.
  BucketMetadata& set_project_number(std::int64_t v) {
    project_number_ = v;
    return *this;
  }

  /// Returns a HTTP link to retrieve the bucket metadata.
  std::string const& self_link() const { return self_link_; }

  /// @note this is only intended for mocking.
  BucketMetadata& set_self_link(std::string v) {
    self_link_ = std::move(v);
    return *this;
  }

  /// @name Accessors and modifiers for retention policy configuration.
  ///@{
  bool has_retention_policy() const { return retention_policy_.has_value(); }
  BucketRetentionPolicy const& retention_policy() const {
    return *retention_policy_;
  }
  absl::optional<BucketRetentionPolicy> const& retention_policy_as_optional()
      const {
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
    return set_retention_policy(BucketRetentionPolicy{
        retention_period, std::chrono::system_clock::time_point{}, false});
  }

  BucketMetadata& reset_retention_policy() {
    retention_policy_.reset();
    return *this;
  }
  ///@}

  /// @name Accessors and modifiers for the Recovery Point Objective.
  ///@{
  std::string const& rpo() const { return rpo_; }
  BucketMetadata& set_rpo(std::string v) {
    rpo_ = std::move(v);
    return *this;
  }
  ///@}

  /// @name Access and modify the default storage class attribute.
  ///@{
  std::string const& storage_class() const { return storage_class_; }
  BucketMetadata& set_storage_class(std::string v) {
    storage_class_ = std::move(v);
    return *this;
  }
  ///@}

  /// Returns the bucket creation timestamp.
  std::chrono::system_clock::time_point time_created() const {
    return time_created_;
  }

  /// @note This is only intended for mocking.
  BucketMetadata& set_time_created(std::chrono::system_clock::time_point v) {
    time_created_ = v;
    return *this;
  }

  /// Returns the timestamp for the last bucket metadata update.
  std::chrono::system_clock::time_point updated() const { return updated_; }

  /// @note This is only intended for mocking.
  BucketMetadata& set_updated(std::chrono::system_clock::time_point v) {
    updated_ = v;
    return *this;
  }

  /// @name Accessors and modifiers for versioning configuration.
  ///@{
  absl::optional<BucketVersioning> const& versioning() const {
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
  BucketMetadata& set_versioning(absl::optional<BucketVersioning> v) {
    versioning_ = std::move(v);
    return *this;
  }
  ///@}

  /// @name Accessors and modifiers for website configuration.
  ///@{
  bool has_website() const { return website_.has_value(); }
  BucketWebsite const& website() const { return *website_; }
  absl::optional<BucketWebsite> const& website_as_optional() const {
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
  ///@}

  /// @name Accessors and modifiers for custom placement configuration.
  ///@{
  bool has_custom_placement_config() const {
    return custom_placement_config_.has_value();
  }
  BucketCustomPlacementConfig const& custom_placement_config() const {
    return *custom_placement_config_;
  }
  absl::optional<BucketCustomPlacementConfig> const&
  custom_placement_config_as_optional() const {
    return custom_placement_config_;
  }
  /// Placement configuration can only be set when the bucket is created.
  BucketMetadata& set_custom_placement_config(BucketCustomPlacementConfig v) {
    custom_placement_config_ = std::move(v);
    return *this;
  }
  /// Placement configuration can only be set when the bucket is created.
  BucketMetadata& reset_custom_placement_config() {
    custom_placement_config_.reset();
    return *this;
  }
  ///@}

  friend bool operator==(BucketMetadata const& lhs, BucketMetadata const& rhs);
  friend bool operator!=(BucketMetadata const& lhs, BucketMetadata const& rhs) {
    return !(lhs == rhs);
  }

 private:
  friend std::ostream& operator<<(std::ostream& os, BucketMetadata const& rhs);
  // Keep the fields in alphabetical order.
  std::vector<BucketAccessControl> acl_;
  absl::optional<BucketBilling> billing_;
  std::vector<CorsEntry> cors_;
  absl::optional<BucketCustomPlacementConfig> custom_placement_config_;
  std::vector<ObjectAccessControl> default_acl_;
  bool default_event_based_hold_ = false;
  absl::optional<BucketEncryption> encryption_;
  std::string etag_;
  absl::optional<BucketIamConfiguration> iam_configuration_;
  std::string id_;
  std::string kind_;
  std::map<std::string, std::string> labels_;
  absl::optional<BucketLifecycle> lifecycle_;
  std::string location_;
  std::string location_type_;
  absl::optional<BucketLogging> logging_;
  std::int64_t metageneration_{0};
  std::string name_;
  absl::optional<Owner> owner_;
  std::int64_t project_number_ = 0;
  absl::optional<BucketRetentionPolicy> retention_policy_;
  std::string rpo_;
  std::string self_link_;
  std::string storage_class_;
  std::chrono::system_clock::time_point time_created_;
  std::chrono::system_clock::time_point updated_;
  absl::optional<BucketVersioning> versioning_;
  absl::optional<BucketWebsite> website_;
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
  BucketMetadataPatchBuilder() = default;

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
    return SetRetentionPolicy(BucketRetentionPolicy{
        retention_period, std::chrono::system_clock::time_point{}, false});
  }
  BucketMetadataPatchBuilder& ResetRetentionPolicy();

  BucketMetadataPatchBuilder& SetRpo(std::string const& v);
  BucketMetadataPatchBuilder& ResetRpo();

  BucketMetadataPatchBuilder& SetStorageClass(std::string const& v);
  BucketMetadataPatchBuilder& ResetStorageClass();

  BucketMetadataPatchBuilder& SetVersioning(BucketVersioning const& v);
  BucketMetadataPatchBuilder& ResetVersioning();

  BucketMetadataPatchBuilder& SetWebsite(BucketWebsite const& v);
  BucketMetadataPatchBuilder& ResetWebsite();

 private:
  friend struct internal::PatchBuilderDetails;

  internal::PatchBuilder impl_;
  bool labels_subpatch_dirty_{false};
  internal::PatchBuilder labels_subpatch_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_METADATA_H

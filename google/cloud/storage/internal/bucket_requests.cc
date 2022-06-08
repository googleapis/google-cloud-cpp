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

#include "google/cloud/storage/internal/bucket_requests.h"
#include "google/cloud/storage/internal/bucket_acl_requests.h"
#include "google/cloud/storage/internal/bucket_metadata_parser.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/format_time_point.h"
#include <nlohmann/json.hpp>
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

void DiffAcl(BucketMetadataPatchBuilder& builder, BucketMetadata const& o,
             BucketMetadata const& u) {
  if (o.acl() != u.acl()) builder.SetAcl(u.acl());
}

void DiffBilling(BucketMetadataPatchBuilder& builder, BucketMetadata const& o,
                 BucketMetadata const& u) {
  if (o.billing_as_optional() == u.billing_as_optional()) return;
  if (u.has_billing()) {
    builder.SetBilling(u.billing());
  } else {
    builder.ResetBilling();
  }
}

void DiffCors(BucketMetadataPatchBuilder& builder, BucketMetadata const& o,
              BucketMetadata const& u) {
  if (o.cors() != u.cors()) builder.SetCors(u.cors());
}

void DiffEventBasedHold(BucketMetadataPatchBuilder& builder,
                        BucketMetadata const& o, BucketMetadata const& u) {
  if (o.default_event_based_hold() != u.default_event_based_hold()) {
    builder.SetDefaultEventBasedHold(u.default_event_based_hold());
  }
}

void DiffDefaultObjectAcl(BucketMetadataPatchBuilder& builder,
                          BucketMetadata const& o, BucketMetadata const& u) {
  if (o.default_acl() != u.default_acl()) {
    builder.SetDefaultAcl(u.default_acl());
  }
}

void DiffEncryption(BucketMetadataPatchBuilder& builder,
                    BucketMetadata const& o, BucketMetadata const& u) {
  if (o.encryption_as_optional() == u.encryption_as_optional()) return;
  if (u.has_encryption()) {
    builder.SetEncryption(u.encryption());
  } else {
    builder.ResetEncryption();
  }
}

void DiffIamConfiguration(BucketMetadataPatchBuilder& builder,
                          BucketMetadata const& o, BucketMetadata const& u) {
  if (o.iam_configuration_as_optional() == u.iam_configuration_as_optional()) {
    return;
  }
  if (u.has_iam_configuration()) {
    builder.SetIamConfiguration(u.iam_configuration());
  } else {
    builder.ResetIamConfiguration();
  }
}

void DiffLabels(BucketMetadataPatchBuilder& builder, BucketMetadata const& o,
                BucketMetadata const& u) {
  if (o.labels() == u.labels()) return;
  if (u.labels().empty()) {
    builder.ResetLabels();
    return;
  }
  std::map<std::string, std::string> difference;
  // Find the keys in the original map that are not in the new map:
  std::set_difference(o.labels().begin(), o.labels().end(), u.labels().begin(),
                      u.labels().end(),
                      std::inserter(difference, difference.end()),
                      // We want to compare just keys and ignore values, the
                      // map class provides such a function, so use it:
                      o.labels().value_comp());
  for (auto&& d : difference) {
    builder.ResetLabel(d.first);
  }

  // Find the elements (comparing key and value) in the updated map that
  // are not in the original map:
  difference.clear();
  std::set_difference(u.labels().begin(), u.labels().end(), o.labels().begin(),
                      o.labels().end(),
                      std::inserter(difference, difference.end()));
  for (auto&& d : difference) {
    builder.SetLabel(d.first, d.second);
  }
}

void DiffLifecycle(BucketMetadataPatchBuilder& builder, BucketMetadata const& o,
                   BucketMetadata const& u) {
  if (o.lifecycle_as_optional() == u.lifecycle_as_optional()) return;
  if (u.has_lifecycle()) {
    builder.SetLifecycle(u.lifecycle());
  } else {
    builder.ResetLifecycle();
  }
}

void DiffLogging(BucketMetadataPatchBuilder& builder, BucketMetadata const& o,
                 BucketMetadata const& u) {
  if (o.logging_as_optional() == u.logging_as_optional()) return;
  if (u.has_logging()) {
    builder.SetLogging(u.logging());
  } else {
    builder.ResetLogging();
  }
}

void DiffName(BucketMetadataPatchBuilder& builder, BucketMetadata const& o,
              BucketMetadata const& u) {
  if (o.name() != u.name()) builder.SetName(u.name());
}

void DiffRetentionPolicy(BucketMetadataPatchBuilder& builder,
                         BucketMetadata const& o, BucketMetadata const& u) {
  if (o.retention_policy_as_optional() == u.retention_policy_as_optional()) {
    return;
  }
  if (u.has_retention_policy()) {
    builder.SetRetentionPolicy(u.retention_policy());
  } else {
    builder.ResetRetentionPolicy();
  }
}

void DiffStorageClass(BucketMetadataPatchBuilder& builder,
                      BucketMetadata const& o, BucketMetadata const& u) {
  if (o.storage_class() != u.storage_class()) {
    builder.SetStorageClass(u.storage_class());
  }
}

void DiffRpo(BucketMetadataPatchBuilder& builder, BucketMetadata const& o,
             BucketMetadata const& u) {
  if (o.rpo() != u.rpo()) builder.SetRpo(u.rpo());
}

void DiffVersioning(BucketMetadataPatchBuilder& builder,
                    BucketMetadata const& o, BucketMetadata const& u) {
  if (o.versioning() == u.versioning()) return;
  if (u.has_versioning()) {
    builder.SetVersioning(*u.versioning());
  } else {
    builder.ResetVersioning();
  }
}

void DiffWebsite(BucketMetadataPatchBuilder& builder, BucketMetadata const& o,
                 BucketMetadata const& u) {
  if (o.website_as_optional() == u.website_as_optional()) return;
  if (u.has_website()) {
    builder.SetWebsite(u.website());
  } else {
    builder.ResetWebsite();
  }
}

BucketMetadataPatchBuilder DiffBucketMetadata(BucketMetadata const& original,
                                              BucketMetadata const& updated) {
  // Compare each modifiable field to build the patch
  BucketMetadataPatchBuilder builder;
  DiffAcl(builder, original, updated);
  DiffBilling(builder, original, updated);
  DiffCors(builder, original, updated);
  DiffEventBasedHold(builder, original, updated);
  DiffDefaultObjectAcl(builder, original, updated);
  DiffEncryption(builder, original, updated);
  DiffIamConfiguration(builder, original, updated);
  DiffLabels(builder, original, updated);
  DiffLifecycle(builder, original, updated);
  DiffLogging(builder, original, updated);
  DiffName(builder, original, updated);
  DiffRetentionPolicy(builder, original, updated);
  DiffRpo(builder, original, updated);
  DiffStorageClass(builder, original, updated);
  DiffVersioning(builder, original, updated);
  DiffWebsite(builder, original, updated);
  return builder;
}

}  // namespace

std::string CreateBucketRequest::json_payload() const {
  return BucketMetadataToJsonString(metadata_);
}

std::string UpdateBucketRequest::json_payload() const {
  return BucketMetadataToJsonString(metadata_);
}

std::ostream& operator<<(std::ostream& os, ListBucketsRequest const& r) {
  os << "ListBucketsRequest={project_id=" << r.project_id();
  r.DumpOptions(os, ", ");
  return os << "}";
}

StatusOr<ListBucketsResponse> ListBucketsResponse::FromHttpResponse(
    std::string const& payload) {
  auto json = nlohmann::json::parse(payload, nullptr, false);
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }

  ListBucketsResponse result;
  result.next_page_token = json.value("nextPageToken", "");

  for (auto const& kv : json["items"].items()) {
    auto parsed = internal::BucketMetadataParser::FromJson(kv.value());
    if (!parsed) {
      return std::move(parsed).status();
    }
    result.items.emplace_back(std::move(*parsed));
  }

  return result;
}

StatusOr<ListBucketsResponse> ListBucketsResponse::FromHttpResponse(
    HttpResponse const& response) {
  return FromHttpResponse(response.payload);
}

std::ostream& operator<<(std::ostream& os, ListBucketsResponse const& r) {
  os << "ListBucketsResponse={next_page_token=" << r.next_page_token
     << ", items={";
  std::copy(r.items.begin(), r.items.end(),
            std::ostream_iterator<BucketMetadata>(os, "\n  "));
  return os << "}}";
}

std::ostream& operator<<(std::ostream& os, GetBucketMetadataRequest const& r) {
  os << "GetBucketMetadataRequest={bucket_name=" << r.bucket_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, CreateBucketRequest const& r) {
  os << "CreateBucketRequest={project_id=" << r.project_id()
     << ", metadata=" << r.metadata();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, DeleteBucketRequest const& r) {
  os << "DeleteBucketRequest={bucket_name=" << r.bucket_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, UpdateBucketRequest const& r) {
  os << "UpdateBucketRequest={metadata=" << r.metadata();
  r.DumpOptions(os, ", ");
  return os << "}";
}

PatchBucketRequest::PatchBucketRequest(std::string bucket,
                                       BucketMetadata const& original,
                                       BucketMetadata const& updated)
    : PatchBucketRequest(std::move(bucket),
                         DiffBucketMetadata(original, updated)) {}

PatchBucketRequest::PatchBucketRequest(std::string bucket,
                                       BucketMetadataPatchBuilder patch)
    : patch_(std::move(patch)), bucket_(std::move(bucket)) {}

std::ostream& operator<<(std::ostream& os, PatchBucketRequest const& r) {
  os << "PatchBucketRequest={bucket_name=" << r.bucket();
  r.DumpOptions(os, ", ");
  return os << ", payload=" << r.payload() << "}";
}

std::ostream& operator<<(std::ostream& os, GetBucketIamPolicyRequest const& r) {
  os << "GetBucketIamPolicyRequest={bucket_name=" << r.bucket_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

SetNativeBucketIamPolicyRequest::SetNativeBucketIamPolicyRequest(
    std::string bucket_name, NativeIamPolicy const& policy)
    : bucket_name_(std::move(bucket_name)),
      policy_(policy),
      json_payload_(policy.ToJson()) {
  if (!policy.etag().empty()) {
    set_option(IfMatchEtag(policy.etag()));
  }
}

std::ostream& operator<<(std::ostream& os,
                         SetNativeBucketIamPolicyRequest const& r) {
  os << "SetNativeBucketIamPolicyRequest={bucket_name=" << r.bucket_name();
  r.DumpOptions(os, ", ");
  return os << ", json_payload=" << r.json_payload() << "}";
}

std::ostream& operator<<(std::ostream& os,
                         TestBucketIamPermissionsRequest const& r) {
  os << "TestBucketIamPermissionsRequest={bucket_name=" << r.bucket_name()
     << ", permissions=[";
  os << absl::StrJoin(r.permissions(), ", ");
  os << "]";
  r.DumpOptions(os, ", ");
  return os << "}";
}

StatusOr<TestBucketIamPermissionsResponse>
TestBucketIamPermissionsResponse::FromHttpResponse(std::string const& payload) {
  TestBucketIamPermissionsResponse result;
  auto json = nlohmann::json::parse(payload, nullptr, false);
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  for (auto const& kv : json["permissions"].items()) {
    result.permissions.emplace_back(kv.value().get<std::string>());
  }
  return result;
}

StatusOr<TestBucketIamPermissionsResponse>
TestBucketIamPermissionsResponse::FromHttpResponse(
    HttpResponse const& response) {
  return FromHttpResponse(response.payload);
}

std::ostream& operator<<(std::ostream& os,
                         TestBucketIamPermissionsResponse const& r) {
  os << "TestBucketIamPermissionsResponse={permissions=[";
  os << absl::StrJoin(r.permissions, ", ");
  return os << "]}";
}

std::ostream& operator<<(std::ostream& os,
                         LockBucketRetentionPolicyRequest const& r) {
  os << "LockBucketRetentionPolicyRequest={bucket_name=" << r.bucket_name()
     << ", metageneration=" << r.metageneration();
  r.DumpOptions(os, ", ");
  return os << "}";
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

// Copyright 2022 Google LLC
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

#include "google/cloud/storage/internal/grpc_bucket_request_parser.h"
#include "google/cloud/storage/internal/bucket_access_control_parser.h"
#include "google/cloud/storage/internal/grpc_bucket_access_control_parser.h"
#include "google/cloud/storage/internal/grpc_bucket_metadata_parser.h"
#include "google/cloud/storage/internal/grpc_common_request_params.h"
#include "google/cloud/storage/internal/grpc_object_access_control_parser.h"
#include "google/cloud/storage/internal/grpc_predefined_acl_parser.h"
#include "google/cloud/storage/internal/lifecycle_rule_parser.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/internal/patch_builder_details.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

google::storage::v2::DeleteBucketRequest GrpcBucketRequestParser::ToProto(
    DeleteBucketRequest const& request) {
  google::storage::v2::DeleteBucketRequest result;
  SetCommonParameters(result, request);
  result.set_name("projects/_/buckets/" + request.bucket_name());
  if (request.HasOption<IfMetagenerationMatch>()) {
    result.set_if_metageneration_match(
        request.GetOption<IfMetagenerationMatch>().value());
  }
  if (request.HasOption<IfMetagenerationNotMatch>()) {
    result.set_if_metageneration_not_match(
        request.GetOption<IfMetagenerationNotMatch>().value());
  }
  return result;
}

google::storage::v2::GetBucketRequest GrpcBucketRequestParser::ToProto(
    GetBucketMetadataRequest const& request) {
  google::storage::v2::GetBucketRequest result;
  SetCommonParameters(result, request);
  result.set_name("projects/_/buckets/" + request.bucket_name());
  if (request.HasOption<IfMetagenerationMatch>()) {
    result.set_if_metageneration_match(
        request.GetOption<IfMetagenerationMatch>().value());
  }
  if (request.HasOption<IfMetagenerationNotMatch>()) {
    result.set_if_metageneration_not_match(
        request.GetOption<IfMetagenerationNotMatch>().value());
  }
  auto projection = request.GetOption<Projection>().value_or("");
  if (projection == "full") result.mutable_read_mask()->add_paths("*");
  return result;
}

google::storage::v2::CreateBucketRequest GrpcBucketRequestParser::ToProto(
    CreateBucketRequest const& request) {
  google::storage::v2::CreateBucketRequest result;
  result.set_parent("projects/" + request.project_id());
  result.set_bucket_id(request.metadata().name());
  if (request.HasOption<PredefinedAcl>()) {
    result.set_predefined_acl(GrpcPredefinedAclParser::ToProtoBucket(
        request.GetOption<PredefinedAcl>()));
  }
  if (request.HasOption<PredefinedDefaultObjectAcl>()) {
    result.set_predefined_default_object_acl(
        GrpcPredefinedAclParser::ToProtoObject(
            request.GetOption<PredefinedDefaultObjectAcl>()));
  }
  *result.mutable_bucket() =
      GrpcBucketMetadataParser::ToProto(request.metadata());
  // Ignore fields commonly set by ToProto().
  result.mutable_bucket()->set_name("");
  result.mutable_bucket()->set_bucket_id("");
  result.mutable_bucket()->clear_create_time();
  result.mutable_bucket()->clear_update_time();
  result.mutable_bucket()->clear_project();
  return result;
}

google::storage::v2::ListBucketsRequest GrpcBucketRequestParser::ToProto(
    ListBucketsRequest const& request) {
  google::storage::v2::ListBucketsRequest result;
  SetCommonParameters(result, request);
  result.set_parent("projects/" + request.project_id());
  auto const page_size = request.GetOption<MaxResults>().value_or(0);
  // Clamp out of range values. The service will clamp to its own range
  // ([0, 1000] as of this writing) anyway.
  if (page_size < 0) {
    result.set_page_size(0);
  } else if (page_size < std::numeric_limits<std::int32_t>::max()) {
    result.set_page_size(static_cast<std::int32_t>(page_size));
  } else {
    result.set_page_size(std::numeric_limits<std::int32_t>::max());
  }
  result.set_page_token(request.page_token());
  result.set_prefix(request.GetOption<Prefix>().value_or(""));
  if (request.GetOption<Projection>().value_or("") == "full") {
    result.mutable_read_mask()->add_paths("*");
  }
  return result;
}

ListBucketsResponse GrpcBucketRequestParser::FromProto(
    google::storage::v2::ListBucketsResponse const& response) {
  ListBucketsResponse result;
  result.next_page_token = response.next_page_token();
  result.items.reserve(response.buckets_size());
  std::transform(response.buckets().begin(), response.buckets().end(),
                 std::back_inserter(result.items),
                 [](google::storage::v2::Bucket const& b) {
                   return GrpcBucketMetadataParser::FromProto(b);
                 });
  return result;
}

StatusOr<google::storage::v2::UpdateBucketRequest>
GrpcBucketRequestParser::ToProto(PatchBucketRequest const& request) {
  google::storage::v2::UpdateBucketRequest result;

  auto& bucket = *result.mutable_bucket();
  bucket.set_name("projects/_/buckets/" + request.bucket());

  auto const& patch = PatchBuilderDetails::GetPatch(request.patch().impl_);

  // The `labels` field is too special, handle separately
  if (request.patch().labels_subpatch_dirty_) {
    // The semantics in gRPC are to replace any labels
    auto const& subpatch =
        PatchBuilderDetails::GetPatch(request.patch().labels_subpatch_);
    for (auto const& kv : subpatch.items()) {
      auto const& v = kv.value();
      if (!v.is_string()) continue;
      (*bucket.mutable_labels())[kv.key()] = v.get<std::string>();
    }
    result.mutable_update_mask()->add_paths("labels");
  }

  // This struct and the vector refactors some common code to create patches for
  // each field.
  using google::storage::v2::Bucket;
  struct Field {
    std::string name;
    std::string rename;
    std::function<Status(Bucket&, nlohmann::json const&)> action;
  } fields[] = {
      // To ease inspection, the fields appear in the same order as they are
      // declared in the proto
      {"storageClass", "storage_class",
       [](Bucket& b, nlohmann::json const& patch) {
         b.set_storage_class(patch.get<std::string>());
         return Status{};
       }},
      {"rpo", "",
       [](Bucket& b, nlohmann::json const& patch) {
         b.set_rpo(patch.get<std::string>());
         return Status{};
       }},
      {"acl", "",
       [](Bucket& b, nlohmann::json const& patch) {
         for (auto const& a : patch) {
           auto& acl = *b.add_acl();
           acl.set_entity(a.value("entity", ""));
           acl.set_role(a.value("role", ""));
         }
         return Status{};
       }},
      {"defaultObjectAcl", "default_object_acl",
       [](Bucket& b, nlohmann::json const& patch) {
         for (auto const& a : patch) {
           auto& acl = *b.add_default_object_acl();
           acl.set_entity(a.value("entity", ""));
           acl.set_role(a.value("role", ""));
         }
         return Status{};
       }},
      {"lifecycle", "",
       [](Bucket& b, nlohmann::json const& patch) {
         auto& lifecycle = *b.mutable_lifecycle();
         // By construction, the PatchBuilder always includes the "rule"
         // subobject.
         for (auto const& r : patch["rule"]) {
           auto lf = LifecycleRuleParser::FromJson(r);
           if (!lf) return std::move(lf).status();
           *lifecycle.add_rule() = GrpcBucketMetadataParser::ToProto(*lf);
         }
         return Status{};
       }},
      {"cors", "",
       [](Bucket& b, nlohmann::json const& patch) {
         for (auto const& c : patch) {
           auto& cors = *b.add_cors();
           cors.set_max_age_seconds(c.value("maxAgeSeconds", std::int32_t{0}));
           if (c.contains("origin")) {
             for (auto const& o : c["origin"]) {
               cors.add_origin(o.get<std::string>());
             }
           }
           if (c.contains("method")) {
             for (auto const& m : c["method"]) {
               cors.add_method(m.get<std::string>());
             }
           }
           if (c.contains("responseHeader")) {
             for (auto const& h : c["responseHeader"]) {
               cors.add_response_header(h.get<std::string>());
             }
           }
         }
         return Status{};
       }},
      {"defaultEventBasedHold", "default_event_based_hold",
       [](Bucket& b, nlohmann::json const& patch) {
         b.set_default_event_based_hold(patch.get<bool>());
         return Status{};
       }},
      {"website", "",
       [](Bucket& b, nlohmann::json const& w) {
         b.mutable_website()->set_main_page_suffix(
             w.value("mainPageSuffix", ""));
         b.mutable_website()->set_not_found_page(w.value("notFoundPage", ""));
         return Status{};
       }},
      {"versioning", "",
       [](Bucket& b, nlohmann::json const& v) {
         b.mutable_versioning()->set_enabled(v.value("enabled", false));
         return Status{};
       }},
      {"logging", "",
       [](Bucket& b, nlohmann::json const& l) {
         b.mutable_logging()->set_log_bucket(l.value("logBucket", ""));
         b.mutable_logging()->set_log_object_prefix(
             l.value("logObjectPrefix", ""));
         return Status{};
       }},
      {"encryption", "",
       [](Bucket& b, nlohmann::json const& e) {
         b.mutable_encryption()->set_default_kms_key(
             e.value("defaultKmsKeyName", ""));
         return Status{};
       }},
      {"billing", "",
       [](Bucket& bucket, nlohmann::json const& b) {
         bucket.mutable_billing()->set_requester_pays(
             b.value("requesterPays", false));
         return Status{};
       }},
      {"retentionPolicy", "retention_policy",
       [](Bucket& b, nlohmann::json const& r) {
         b.mutable_retention_policy()->set_retention_period(
             r.value("retentionPeriod", int64_t{0}));
         return Status{};
       }},
      {"iamConfiguration", "iam_config",
       [](Bucket& b, nlohmann::json const& i) {
         auto& iam_config = *b.mutable_iam_config();
         if (i.contains("uniformBucketLevelAccess")) {
           iam_config.mutable_uniform_bucket_level_access()->set_enabled(
               i["uniformBucketLevelAccess"].value("enabled", false));
         }
         if (i.contains("publicAccessPrevention")) {
           auto pap = i.value("publicAccessPrevention", "");
           iam_config.set_public_access_prevention(
               GrpcBucketMetadataParser::ToProtoPublicAccessPrevention(pap));
         }
         return Status{};
       }},
  };

  for (auto const& field : fields) {
    if (!patch.contains(field.name)) continue;
    auto status = field.action(bucket, patch[field.name]);
    if (!status.ok()) return status;
    result.mutable_update_mask()->add_paths(
        field.rename.empty() ? field.name : field.rename);
  }

  if (request.HasOption<IfMetagenerationMatch>()) {
    result.set_if_metageneration_match(
        request.GetOption<IfMetagenerationMatch>().value());
  }
  if (request.HasOption<IfMetagenerationNotMatch>()) {
    result.set_if_metageneration_not_match(
        request.GetOption<IfMetagenerationNotMatch>().value());
  }
  if (request.HasOption<PredefinedAcl>()) {
    result.set_predefined_acl(GrpcPredefinedAclParser::ToProtoBucket(
        request.GetOption<PredefinedAcl>()));
  }
  if (request.HasOption<PredefinedDefaultObjectAcl>()) {
    result.set_predefined_default_object_acl(
        GrpcPredefinedAclParser::ToProtoObject(
            request.GetOption<PredefinedDefaultObjectAcl>()));
  }
  SetCommonParameters(result, request);

  return result;
}

google::storage::v2::UpdateBucketRequest GrpcBucketRequestParser::ToProto(
    UpdateBucketRequest const& request) {
  google::storage::v2::UpdateBucketRequest result;

  auto& bucket = *result.mutable_bucket();
  auto const& metadata = request.metadata();
  bucket.set_name("projects/_/buckets/" + metadata.name());

  // We set the update_mask for all fields, even if not present in `metadata` as
  // "not present" implies the field should be cleared.

  result.mutable_update_mask()->add_paths("storage_class");
  bucket.set_storage_class(metadata.storage_class());
  result.mutable_update_mask()->add_paths("rpo");
  bucket.set_rpo(metadata.rpo());
  result.mutable_update_mask()->add_paths("acl");
  for (auto const& a : metadata.acl()) {
    auto& acl = *bucket.add_acl();
    acl.set_entity(a.entity());
    acl.set_role(a.role());
  }
  result.mutable_update_mask()->add_paths("default_object_acl");
  for (auto const& a : metadata.default_acl()) {
    auto& acl = *bucket.add_default_object_acl();
    acl.set_entity(a.entity());
    acl.set_role(a.role());
  }
  result.mutable_update_mask()->add_paths("lifecycle");
  if (metadata.has_lifecycle()) {
    auto& lifecycle = *bucket.mutable_lifecycle();
    // By construction, the PatchBuilder always includes the "rule"
    // subobject.
    for (auto const& r : metadata.lifecycle().rule) {
      *lifecycle.add_rule() = GrpcBucketMetadataParser::ToProto(r);
    }
  }
  result.mutable_update_mask()->add_paths("cors");
  for (auto const& c : metadata.cors()) {
    auto& cors = *bucket.add_cors();
    cors.set_max_age_seconds(
        static_cast<std::int32_t>(c.max_age_seconds.value_or(0)));
    for (auto const& o : c.origin) {
      cors.add_origin(o);
    }
    for (auto const& m : c.method) {
      cors.add_method(m);
    }
    for (auto const& h : c.response_header) {
      cors.add_response_header(h);
    }
  }
  result.mutable_update_mask()->add_paths("default_event_based_hold");
  bucket.set_default_event_based_hold(metadata.default_event_based_hold());
  result.mutable_update_mask()->add_paths("labels");
  for (auto const& kv : metadata.labels()) {
    (*bucket.mutable_labels())[kv.first] = kv.second;
  }
  result.mutable_update_mask()->add_paths("website");
  if (metadata.has_website()) {
    auto const& w = metadata.website();
    bucket.mutable_website()->set_main_page_suffix(w.main_page_suffix);
    bucket.mutable_website()->set_not_found_page(w.not_found_page);
  }
  result.mutable_update_mask()->add_paths("versioning");
  if (metadata.has_versioning()) {
    bucket.mutable_versioning()->set_enabled(metadata.versioning()->enabled);
  }
  result.mutable_update_mask()->add_paths("logging");
  if (metadata.has_logging()) {
    bucket.mutable_logging()->set_log_bucket(metadata.logging().log_bucket);
    bucket.mutable_logging()->set_log_object_prefix(
        metadata.logging().log_object_prefix);
  }
  result.mutable_update_mask()->add_paths("encryption");
  if (metadata.has_encryption()) {
    bucket.mutable_encryption()->set_default_kms_key(
        metadata.encryption().default_kms_key_name);
  }
  result.mutable_update_mask()->add_paths("billing");
  if (metadata.has_billing()) {
    bucket.mutable_billing()->set_requester_pays(
        metadata.billing().requester_pays);
  }
  result.mutable_update_mask()->add_paths("retention_policy");
  if (metadata.has_retention_policy()) {
    bucket.mutable_retention_policy()->set_retention_period(
        metadata.retention_policy().retention_period.count());
  }
  result.mutable_update_mask()->add_paths("iam_config");
  if (metadata.has_iam_configuration()) {
    auto& iam_config = *bucket.mutable_iam_config();
    auto const& i = metadata.iam_configuration();
    if (i.uniform_bucket_level_access.has_value()) {
      iam_config.mutable_uniform_bucket_level_access()->set_enabled(
          i.uniform_bucket_level_access->enabled);
    }
    if (i.public_access_prevention.has_value()) {
      auto pap = i.public_access_prevention.value();
      iam_config.set_public_access_prevention(
          GrpcBucketMetadataParser::ToProtoPublicAccessPrevention(pap));
    }
  }

  if (request.HasOption<IfMetagenerationMatch>()) {
    result.set_if_metageneration_match(
        request.GetOption<IfMetagenerationMatch>().value());
  }
  if (request.HasOption<IfMetagenerationNotMatch>()) {
    result.set_if_metageneration_not_match(
        request.GetOption<IfMetagenerationNotMatch>().value());
  }
  if (request.HasOption<PredefinedAcl>()) {
    result.set_predefined_acl(GrpcPredefinedAclParser::ToProtoBucket(
        request.GetOption<PredefinedAcl>()));
  }
  if (request.HasOption<PredefinedDefaultObjectAcl>()) {
    result.set_predefined_default_object_acl(
        GrpcPredefinedAclParser::ToProtoObject(
            request.GetOption<PredefinedDefaultObjectAcl>()));
  }
  SetCommonParameters(result, request);

  return result;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

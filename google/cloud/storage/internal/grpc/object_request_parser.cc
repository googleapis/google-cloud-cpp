// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/grpc/object_request_parser.h"
#include "google/cloud/storage/internal/base64.h"
#include "google/cloud/storage/internal/grpc/bucket_name.h"
#include "google/cloud/storage/internal/grpc/object_access_control_parser.h"
#include "google/cloud/storage/internal/grpc/object_metadata_parser.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/internal/patch_builder_details.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/time_utils.h"
#include <iterator>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::storage::v2::Object;

template <typename GrpcRequest, typename StorageRequest>
Status SetCommonObjectParameters(GrpcRequest& request,
                                 StorageRequest const& req) {
  if (req.template HasOption<storage::EncryptionKey>()) {
    auto data = req.template GetOption<storage::EncryptionKey>().value();
    auto key_bytes = storage::internal::Base64Decode(data.key);
    if (!key_bytes) return std::move(key_bytes).status();
    auto key_sha256_bytes = storage::internal::Base64Decode(data.sha256);
    if (!key_sha256_bytes) return std::move(key_sha256_bytes).status();

    request.mutable_common_object_request_params()->set_encryption_algorithm(
        std::move(data.algorithm));
    request.mutable_common_object_request_params()->set_encryption_key_bytes(
        std::string{key_bytes->begin(), key_bytes->end()});
    request.mutable_common_object_request_params()
        ->set_encryption_key_sha256_bytes(
            std::string{key_sha256_bytes->begin(), key_sha256_bytes->end()});
  }
  return Status{};
}

template <typename GrpcRequest>
struct GetPredefinedAcl {
  auto operator()(GrpcRequest const& q) -> decltype(q.predefined_acl());
};

template <
    typename GrpcRequest, typename StorageRequest,
    std::enable_if_t<
        std::is_same<std::string const&,
                     google::cloud::internal::invoke_result_t<
                         GetPredefinedAcl<GrpcRequest>, GrpcRequest>>::value,
        int> = 0>
void SetPredefinedAcl(GrpcRequest& request, StorageRequest const& req) {
  if (req.template HasOption<storage::PredefinedAcl>()) {
    request.set_predefined_acl(
        req.template GetOption<storage::PredefinedAcl>().value());
  }
}

template <typename GrpcRequest, typename StorageRequest>
void SetMetagenerationConditions(GrpcRequest& request,
                                 StorageRequest const& req) {
  if (req.template HasOption<storage::IfMetagenerationMatch>()) {
    request.set_if_metageneration_match(
        req.template GetOption<storage::IfMetagenerationMatch>().value());
  }
  if (req.template HasOption<storage::IfMetagenerationNotMatch>()) {
    request.set_if_metageneration_not_match(
        req.template GetOption<storage::IfMetagenerationNotMatch>().value());
  }
}

template <typename GrpcRequest, typename StorageRequest>
void SetGenerationConditions(GrpcRequest& request, StorageRequest const& req) {
  if (req.template HasOption<storage::IfGenerationMatch>()) {
    request.set_if_generation_match(
        req.template GetOption<storage::IfGenerationMatch>().value());
  }
  if (req.template HasOption<storage::IfGenerationNotMatch>()) {
    request.set_if_generation_not_match(
        req.template GetOption<storage::IfGenerationNotMatch>().value());
  }
}

template <typename StorageRequest>
void SetResourceOptions(google::storage::v2::Object& resource,
                        StorageRequest const& request) {
  if (request.template HasOption<storage::ContentEncoding>()) {
    resource.set_content_encoding(
        request.template GetOption<storage::ContentEncoding>().value());
  }
  if (request.template HasOption<storage::ContentType>()) {
    resource.set_content_type(
        request.template GetOption<storage::ContentType>().value());
  }
  if (request.template HasOption<storage::KmsKeyName>()) {
    resource.set_kms_key(
        request.template GetOption<storage::KmsKeyName>().value());
  }
}

template <typename StorageRequest>
Status SetObjectMetadata(google::storage::v2::Object& resource,
                         StorageRequest const& req) {
  if (!req.template HasOption<storage::WithObjectMetadata>()) {
    return Status{};
  }
  auto metadata = req.template GetOption<storage::WithObjectMetadata>().value();
  if (!metadata.content_encoding().empty()) {
    resource.set_content_encoding(metadata.content_encoding());
  }
  if (!metadata.content_disposition().empty()) {
    resource.set_content_disposition(metadata.content_disposition());
  }
  if (!metadata.cache_control().empty()) {
    resource.set_cache_control(metadata.cache_control());
  }
  for (auto const& acl : metadata.acl()) {
    *resource.add_acl() = storage_internal::ToProto(acl);
  }
  if (!metadata.content_language().empty()) {
    resource.set_content_language(metadata.content_language());
  }
  if (!metadata.content_type().empty()) {
    resource.set_content_type(metadata.content_type());
  }
  resource.set_temporary_hold(metadata.temporary_hold());
  for (auto const& kv : metadata.metadata()) {
    (*resource.mutable_metadata())[kv.first] = kv.second;
  }
  if (metadata.event_based_hold()) {
    resource.set_event_based_hold(metadata.event_based_hold());
  }
  // The customer_encryption field is never set via the object resource, gRPC
  // defines a separate message (`CommonObjectRequestParams`) and field in each
  // request to include the encryption info.
  // *resource.mutable_customer_encryption() = ...;
  if (metadata.has_custom_time()) {
    *resource.mutable_custom_time() =
        google::cloud::internal::ToProtoTimestamp(metadata.custom_time());
  }
  return Status{};
}

// Only a few requests can set the storage class of the destination Object.
template <typename StorageRequest>
void SetStorageClass(google::storage::v2::Object& resource,
                     StorageRequest const& req) {
  if (!req.template HasOption<storage::WithObjectMetadata>()) return;
  auto metadata = req.template GetOption<storage::WithObjectMetadata>().value();
  resource.set_storage_class(metadata.storage_class());
}

Status PatchAcl(Object& o, nlohmann::json const& p) {
  if (p.is_null()) {
    o.clear_acl();
    return Status{};
  }
  for (auto const& a : p) {
    auto acl = storage::internal::ObjectAccessControlParser::FromJson(a);
    // We do not care if `o` may have been modified. It will be discarded if
    // this function (or similar functions) return a non-Okay Status.
    if (!acl) return std::move(acl).status();
    *o.add_acl() = storage_internal::ToProto(*acl);
  }
  return Status{};
}

Status PatchCustomTime(Object& o, nlohmann::json const& p) {
  if (p.is_null()) {
    o.clear_custom_time();
    return Status{};
  }
  auto ts = google::cloud::internal::ParseRfc3339(p.get<std::string>());
  if (!ts) return std::move(ts).status();
  *o.mutable_custom_time() = google::cloud::internal::ToProtoTimestamp(*ts);
  return Status{};
}

Status PatchEventBasedHold(Object& o, nlohmann::json const& p) {
  if (p.is_null()) {
    o.clear_event_based_hold();
  } else {
    o.set_event_based_hold(p.get<bool>());
  }
  return Status{};
}

Status PatchTemporaryHold(Object& o, nlohmann::json const& p) {
  if (p.is_null()) {
    o.clear_temporary_hold();
  } else {
    o.set_temporary_hold(p.get<bool>());
  }
  return Status{};
}

template <typename Request>
StatusOr<google::storage::v2::WriteObjectRequest> ToProtoImpl(
    Request const& request) {
  google::storage::v2::WriteObjectRequest r;
  auto& object_spec = *r.mutable_write_object_spec();
  auto& resource = *object_spec.mutable_resource();
  SetResourceOptions(resource, request);
  auto status = SetObjectMetadata(resource, request);
  if (!status.ok()) return status;
  SetStorageClass(resource, request);
  SetPredefinedAcl(object_spec, request);
  SetGenerationConditions(object_spec, request);
  SetMetagenerationConditions(object_spec, request);
  status = SetCommonObjectParameters(r, request);
  if (!status.ok()) return status;

  resource.set_bucket(GrpcBucketIdToName(request.bucket_name()));
  resource.set_name(request.object_name());
  r.set_write_offset(0);

  return r;
}

Status FinalizeChecksums(google::storage::v2::ObjectChecksums& checksums,
                         storage::internal::HashValues const& hashes) {
  // The client library accepts CRC32C and MD5 checksums in the format required
  // by the REST APIs, that is, base64-encoded. We need to convert this to the
  // format expected by proto, which is just a 32-bit integer for CRC32C and a
  // byte array for MD5.
  //
  // These conversions may fail, because the value is provided by the
  // application in some cases.
  if (!hashes.crc32c.empty()) {
    auto as_proto = storage_internal::Crc32cToProto(hashes.crc32c);
    if (!as_proto) return std::move(as_proto).status();
    checksums.set_crc32c(*as_proto);
  }

  if (!hashes.md5.empty()) {
    auto as_proto = storage_internal::MD5ToProto(hashes.md5);
    if (!as_proto) return std::move(as_proto).status();
    checksums.set_md5_hash(*as_proto);
  }
  return {};
}

}  // namespace

StatusOr<google::storage::v2::ComposeObjectRequest> ToProto(
    storage::internal::ComposeObjectRequest const& request) {
  google::storage::v2::ComposeObjectRequest result;
  auto status = SetCommonObjectParameters(result, request);
  if (!status.ok()) return status;

  auto& destination = *result.mutable_destination();
  destination.set_bucket(GrpcBucketIdToName(request.bucket_name()));
  destination.set_name(request.object_name());
  if (request.HasOption<storage::WithObjectMetadata>()) {
    auto metadata = request.GetOption<storage::WithObjectMetadata>().value();
    for (auto const& a : metadata.acl()) {
      *destination.add_acl() = storage_internal::ToProto(a);
    }
    for (auto const& kv : metadata.metadata()) {
      (*destination.mutable_metadata())[kv.first] = kv.second;
    }
    destination.set_content_encoding(metadata.content_encoding());
    destination.set_content_disposition(metadata.content_disposition());
    destination.set_cache_control(metadata.cache_control());
    destination.set_content_language(metadata.content_language());
    destination.set_content_type(metadata.content_type());
    destination.set_storage_class(metadata.storage_class());
    destination.set_temporary_hold(metadata.temporary_hold());
    destination.set_event_based_hold(metadata.event_based_hold());
    if (metadata.has_custom_time()) {
      *destination.mutable_custom_time() =
          google::cloud::internal::ToProtoTimestamp(metadata.custom_time());
    }
  }
  for (auto const& s : request.source_objects()) {
    google::storage::v2::ComposeObjectRequest::SourceObject source;
    source.set_name(s.object_name);
    source.set_generation(s.generation.value_or(0));
    if (s.if_generation_match.has_value()) {
      source.mutable_object_preconditions()->set_if_generation_match(
          *s.if_generation_match);
    }
    *result.add_source_objects() = std::move(source);
  }
  if (request.HasOption<storage::DestinationPredefinedAcl>()) {
    result.set_destination_predefined_acl(
        request.GetOption<storage::DestinationPredefinedAcl>().value());
  }
  if (request.HasOption<storage::IfGenerationMatch>()) {
    result.set_if_generation_match(
        request.GetOption<storage::IfGenerationMatch>().value());
  }
  if (request.HasOption<storage::IfMetagenerationMatch>()) {
    result.set_if_metageneration_match(
        request.GetOption<storage::IfMetagenerationMatch>().value());
  }
  result.set_kms_key(request.GetOption<storage::KmsKeyName>().value_or(""));
  return result;
}

google::storage::v2::DeleteObjectRequest ToProto(
    storage::internal::DeleteObjectRequest const& request) {
  google::storage::v2::DeleteObjectRequest result;
  SetGenerationConditions(result, request);
  SetMetagenerationConditions(result, request);
  result.set_bucket(GrpcBucketIdToName(request.bucket_name()));
  result.set_object(request.object_name());
  result.set_generation(request.GetOption<storage::Generation>().value_or(0));
  return result;
}

google::storage::v2::GetObjectRequest ToProto(
    storage::internal::GetObjectMetadataRequest const& request) {
  google::storage::v2::GetObjectRequest result;
  SetGenerationConditions(result, request);
  SetMetagenerationConditions(result, request);

  result.set_bucket(GrpcBucketIdToName(request.bucket_name()));
  result.set_object(request.object_name());
  result.set_generation(request.GetOption<storage::Generation>().value_or(0));
  auto projection = request.GetOption<storage::Projection>().value_or("");
  if (projection == "full") result.mutable_read_mask()->add_paths("*");
  if (request.GetOption<storage::SoftDeleted>().value_or(false)) {
    result.set_soft_deleted(true);
  }
  return result;
}

StatusOr<google::storage::v2::ReadObjectRequest> ToProto(
    storage::internal::ReadObjectRangeRequest const& request) {
  // With the REST API this condition was detected by the server as an error.
  // Generally we prefer the server to detect errors because its answers are
  // authoritative, but in this case it cannot. With gRPC, '0' is the same as
  // "not set" so the server would send back the full file, and that is unlikely
  // to be the customer's intent.
  if (request.HasOption<storage::ReadLast>() &&
      request.GetOption<storage::ReadLast>().value() == 0) {
    return internal::OutOfRangeError(
        "ReadLast(0) is invalid in REST and produces incorrect output in gRPC",
        GCP_ERROR_INFO());
  }
  // We should not guess the intent in this case.
  if (request.HasOption<storage::ReadLast>() &&
      request.HasOption<storage::ReadRange>()) {
    return internal::InvalidArgumentError(
        "Cannot use ReadLast() and ReadRange() at the same time",
        GCP_ERROR_INFO());
  }
  // We should not guess the intent in this case.
  if (request.HasOption<storage::ReadLast>() &&
      request.HasOption<storage::ReadFromOffset>()) {
    return internal::InvalidArgumentError(
        "Cannot use ReadLast() and ReadFromOffset() at the same time",
        GCP_ERROR_INFO());
  }
  google::storage::v2::ReadObjectRequest r;
  auto status = SetCommonObjectParameters(r, request);
  if (!status.ok()) return status;
  r.set_object(request.object_name());
  r.set_bucket(GrpcBucketIdToName(request.bucket_name()));
  if (request.HasOption<storage::Generation>()) {
    r.set_generation(request.GetOption<storage::Generation>().value());
  }
  if (request.HasOption<storage::ReadRange>()) {
    auto const range = request.GetOption<storage::ReadRange>().value();
    r.set_read_offset(range.begin);
    r.set_read_limit(range.end - range.begin);
  }
  if (request.HasOption<storage::ReadLast>()) {
    auto const offset = request.GetOption<storage::ReadLast>().value();
    r.set_read_offset(-offset);
  }
  if (request.HasOption<storage::ReadFromOffset>()) {
    auto const offset = request.GetOption<storage::ReadFromOffset>().value();
    if (offset > r.read_offset()) {
      if (r.read_limit() > 0) {
        r.set_read_limit(offset - r.read_offset());
      }
      r.set_read_offset(offset);
    }
  }
  SetGenerationConditions(r, request);
  SetMetagenerationConditions(r, request);

  return r;
}

StatusOr<google::storage::v2::UpdateObjectRequest> ToProto(
    storage::internal::PatchObjectRequest const& request) {
  google::storage::v2::UpdateObjectRequest result;
  auto status = SetCommonObjectParameters(result, request);
  if (!status.ok()) return status;
  SetGenerationConditions(result, request);
  SetMetagenerationConditions(result, request);
  SetPredefinedAcl(result, request);

  auto& object = *result.mutable_object();
  object.set_bucket(GrpcBucketIdToName(request.bucket_name()));
  object.set_name(request.object_name());
  object.set_generation(request.GetOption<storage::Generation>().value_or(0));

  auto const& patch =
      storage::internal::PatchBuilderDetails::GetPatch(request.patch());
  struct ComplexField {
    char const* json_name;
    char const* grpc_name;
    std::function<Status(Object&, nlohmann::json const&)> action;
  } fields[] = {
      {"acl", "acl", PatchAcl},
      {"customTime", "custom_time", PatchCustomTime},
      {"eventBasedHold", "event_based_hold", PatchEventBasedHold},
      {"temporaryHold", "temporary_hold", PatchTemporaryHold},
  };

  for (auto const& field : fields) {
    if (!patch.contains(field.json_name)) continue;
    auto s = field.action(object, patch[field.json_name]);
    if (!s.ok()) return s;
    result.mutable_update_mask()->add_paths(field.grpc_name);
  }

  auto const& subpatch =
      storage::internal::PatchBuilderDetails::GetMetadataSubPatch(
          request.patch());
  if (subpatch.is_null()) {
    object.clear_metadata();
    result.mutable_update_mask()->add_paths("metadata");
  } else {
    for (auto const& kv : subpatch.items()) {
      result.mutable_update_mask()->add_paths("metadata." + kv.key());
      auto const& v = kv.value();
      if (!v.is_string()) continue;
      (*object.mutable_metadata())[kv.key()] = v.get<std::string>();
    }
  }

  // We need to check each modifiable field.
  struct StringField {
    char const* json_name;
    char const* grpc_name;
    std::function<void(std::string v)> setter;
  } string_fields[] = {
      {"cacheControl", "cache_control",
       [&object](std::string v) { object.set_cache_control(std::move(v)); }},
      {"contentDisposition", "content_disposition",
       [&object](std::string v) {
         object.set_content_disposition(std::move(v));
       }},
      {"contentEncoding", "content_encoding",
       [&object](std::string v) { object.set_content_encoding(std::move(v)); }},
      {"contentLanguage", "content_language",
       [&object](std::string v) { object.set_content_language(std::move(v)); }},
      {"contentType", "content_type",
       [&object](std::string v) { object.set_content_type(std::move(v)); }},
  };
  for (auto const& f : string_fields) {
    if (!patch.contains(f.json_name)) continue;
    auto const& p = patch[f.json_name];
    f.setter(p.is_null() ? std::string{} : p.get<std::string>());
    result.mutable_update_mask()->add_paths(f.grpc_name);
  }

  return result;
}

StatusOr<google::storage::v2::UpdateObjectRequest> ToProto(
    storage::internal::UpdateObjectRequest const& request) {
  google::storage::v2::UpdateObjectRequest result;
  auto status = SetCommonObjectParameters(result, request);
  if (!status.ok()) return status;
  SetGenerationConditions(result, request);
  SetMetagenerationConditions(result, request);
  SetPredefinedAcl(result, request);

  auto& object = *result.mutable_object();
  object.set_bucket(GrpcBucketIdToName(request.bucket_name()));
  object.set_name(request.object_name());
  object.set_generation(request.GetOption<storage::Generation>().value_or(0));

  result.mutable_update_mask()->add_paths("acl");
  for (auto const& a : request.metadata().acl()) {
    *object.add_acl() = storage_internal::ToProto(a);
  }

  // The semantics in gRPC are to replace any metadata attributes
  result.mutable_update_mask()->add_paths("metadata");
  for (auto const& kv : request.metadata().metadata()) {
    (*object.mutable_metadata())[kv.first] = kv.second;
  }

  if (request.metadata().has_custom_time()) {
    result.mutable_update_mask()->add_paths("custom_time");
    *object.mutable_custom_time() = google::cloud::internal::ToProtoTimestamp(
        request.metadata().custom_time());
  }

  // We need to check each modifiable field.
  result.mutable_update_mask()->add_paths("cache_control");
  object.set_cache_control(request.metadata().cache_control());
  result.mutable_update_mask()->add_paths("content_disposition");
  object.set_content_disposition(request.metadata().content_disposition());
  result.mutable_update_mask()->add_paths("content_encoding");
  object.set_content_encoding(request.metadata().content_encoding());
  result.mutable_update_mask()->add_paths("content_language");
  object.set_content_language(request.metadata().content_language());
  result.mutable_update_mask()->add_paths("content_type");
  object.set_content_type(request.metadata().content_type());
  result.mutable_update_mask()->add_paths("event_based_hold");
  object.set_event_based_hold(request.metadata().event_based_hold());
  result.mutable_update_mask()->add_paths("temporary_hold");
  object.set_temporary_hold(request.metadata().temporary_hold());

  return result;
}

StatusOr<google::storage::v2::WriteObjectRequest> ToProto(
    storage::internal::InsertObjectMediaRequest const& request) {
  return ToProtoImpl(request);
}

storage::internal::QueryResumableUploadResponse FromProto(
    google::storage::v2::WriteObjectResponse const& p, Options const& options,
    google::cloud::RpcMetadata metadata) {
  storage::internal::QueryResumableUploadResponse response;
  if (p.has_persisted_size()) {
    response.committed_size = static_cast<std::uint64_t>(p.persisted_size());
  }
  if (p.has_resource()) {
    response.payload = storage_internal::FromProto(p.resource(), options);
  }
  response.request_metadata = std::move(metadata.headers);
  response.request_metadata.insert(
      std::make_move_iterator(metadata.trailers.begin()),
      std::make_move_iterator(metadata.trailers.end()));
  return response;
}

google::storage::v2::ListObjectsRequest ToProto(
    storage::internal::ListObjectsRequest const& request) {
  google::storage::v2::ListObjectsRequest result;
  result.set_parent(GrpcBucketIdToName(request.bucket_name()));
  auto const page_size = request.GetOption<storage::MaxResults>().value_or(0);
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
  result.set_delimiter(request.GetOption<storage::Delimiter>().value_or(""));
  result.set_include_trailing_delimiter(
      request.GetOption<storage::IncludeTrailingDelimiter>().value_or(false));
  result.set_prefix(request.GetOption<storage::Prefix>().value_or(""));
  result.set_versions(request.GetOption<storage::Versions>().value_or(false));
  result.set_lexicographic_start(
      request.GetOption<storage::StartOffset>().value_or(""));
  result.set_lexicographic_end(
      request.GetOption<storage::EndOffset>().value_or(""));
  result.set_match_glob(request.GetOption<storage::MatchGlob>().value_or(""));
  result.set_soft_deleted(
      request.GetOption<storage::SoftDeleted>().value_or(false));
  result.set_include_folders_as_prefixes(
      request.GetOption<storage::IncludeFoldersAsPrefixes>().value_or(false));
  return result;
}

storage::internal::ListObjectsResponse FromProto(
    google::storage::v2::ListObjectsResponse const& response,
    Options const& options) {
  storage::internal::ListObjectsResponse result;
  result.next_page_token = response.next_page_token();
  for (auto const& o : response.objects()) {
    result.items.push_back(storage_internal::FromProto(o, options));
  }
  for (auto const& p : response.prefixes()) result.prefixes.push_back(p);
  return result;
}

StatusOr<google::storage::v2::RewriteObjectRequest> ToProto(
    storage::internal::RewriteObjectRequest const& request) {
  google::storage::v2::RewriteObjectRequest result;
  auto status = SetCommonObjectParameters(result, request);
  if (!status.ok()) return status;

  result.set_destination_name(request.destination_object());
  result.set_destination_bucket(
      GrpcBucketIdToName(request.destination_bucket()));

  if (request.HasOption<storage::WithObjectMetadata>() ||
      request.HasOption<storage::DestinationKmsKeyName>()) {
    auto& destination = *result.mutable_destination();
    destination.set_kms_key(
        request.GetOption<storage::DestinationKmsKeyName>().value_or(""));
    status = SetObjectMetadata(destination, request);
    if (!status.ok()) return status;
    SetStorageClass(destination, request);
  }
  result.set_source_bucket(GrpcBucketIdToName(request.source_bucket()));
  result.set_source_object(request.source_object());
  result.set_source_generation(
      request.GetOption<storage::SourceGeneration>().value_or(0));
  result.set_rewrite_token(request.rewrite_token());
  if (request.HasOption<storage::DestinationPredefinedAcl>()) {
    result.set_destination_predefined_acl(
        request.GetOption<storage::DestinationPredefinedAcl>().value());
  }
  SetGenerationConditions(result, request);
  SetMetagenerationConditions(result, request);
  if (request.HasOption<storage::IfSourceGenerationMatch>()) {
    result.set_if_source_generation_match(
        request.GetOption<storage::IfSourceGenerationMatch>().value());
  }
  if (request.HasOption<storage::IfSourceGenerationNotMatch>()) {
    result.set_if_source_generation_not_match(
        request.GetOption<storage::IfSourceGenerationNotMatch>().value());
  }
  if (request.HasOption<storage::IfSourceMetagenerationMatch>()) {
    result.set_if_source_metageneration_match(
        request.GetOption<storage::IfSourceMetagenerationMatch>().value());
  }
  if (request.HasOption<storage::IfSourceMetagenerationNotMatch>()) {
    result.set_if_source_metageneration_not_match(
        request.GetOption<storage::IfSourceMetagenerationNotMatch>().value());
  }
  result.set_max_bytes_rewritten_per_call(
      request.GetOption<storage::MaxBytesRewrittenPerCall>().value_or(0));
  if (request.HasOption<storage::SourceEncryptionKey>()) {
    auto data =
        request.template GetOption<storage::SourceEncryptionKey>().value();
    auto key_bytes = storage::internal::Base64Decode(data.key);
    if (!key_bytes) return std::move(key_bytes).status();
    auto key_sha256_bytes = storage::internal::Base64Decode(data.sha256);
    if (!key_sha256_bytes) return std::move(key_sha256_bytes).status();

    result.set_copy_source_encryption_algorithm(data.algorithm);
    result.set_copy_source_encryption_key_bytes(
        std::string{key_bytes->begin(), key_bytes->end()});
    result.set_copy_source_encryption_key_sha256_bytes(
        std::string{key_sha256_bytes->begin(), key_sha256_bytes->end()});
  }
  return result;
}

storage::internal::RewriteObjectResponse FromProto(
    google::storage::v2::RewriteResponse const& response,
    Options const& options) {
  storage::internal::RewriteObjectResponse result;
  result.done = response.done();
  result.object_size = response.object_size();
  result.total_bytes_rewritten = response.total_bytes_rewritten();
  result.rewrite_token = response.rewrite_token();
  if (response.has_resource()) {
    result.resource = storage_internal::FromProto(response.resource(), options);
  }
  return result;
}

StatusOr<google::storage::v2::RewriteObjectRequest> ToProto(
    storage::internal::CopyObjectRequest const& request) {
  google::storage::v2::RewriteObjectRequest result;
  auto status = SetCommonObjectParameters(result, request);
  if (!status.ok()) return status;

  result.set_destination_name(request.destination_object());
  result.set_destination_bucket(
      GrpcBucketIdToName(request.destination_bucket()));

  if (request.HasOption<storage::WithObjectMetadata>() ||
      request.HasOption<storage::DestinationKmsKeyName>()) {
    auto& destination = *result.mutable_destination();
    destination.set_kms_key(
        request.GetOption<storage::DestinationKmsKeyName>().value_or(""));
    status = SetObjectMetadata(destination, request);
    if (!status.ok()) return status;
    SetStorageClass(destination, request);
  }
  result.set_source_bucket(GrpcBucketIdToName(request.source_bucket()));
  result.set_source_object(request.source_object());
  result.set_source_generation(
      request.GetOption<storage::SourceGeneration>().value_or(0));
  if (request.HasOption<storage::DestinationPredefinedAcl>()) {
    result.set_destination_predefined_acl(
        request.GetOption<storage::DestinationPredefinedAcl>().value());
  }
  SetGenerationConditions(result, request);
  SetMetagenerationConditions(result, request);
  if (request.HasOption<storage::IfSourceGenerationMatch>()) {
    result.set_if_source_generation_match(
        request.GetOption<storage::IfSourceGenerationMatch>().value());
  }
  if (request.HasOption<storage::IfSourceGenerationNotMatch>()) {
    result.set_if_source_generation_not_match(
        request.GetOption<storage::IfSourceGenerationNotMatch>().value());
  }
  if (request.HasOption<storage::IfSourceMetagenerationMatch>()) {
    result.set_if_source_metageneration_match(
        request.GetOption<storage::IfSourceMetagenerationMatch>().value());
  }
  if (request.HasOption<storage::IfSourceMetagenerationNotMatch>()) {
    result.set_if_source_metageneration_not_match(
        request.GetOption<storage::IfSourceMetagenerationNotMatch>().value());
  }
  if (request.HasOption<storage::SourceEncryptionKey>()) {
    auto data =
        request.template GetOption<storage::SourceEncryptionKey>().value();
    auto key_bytes = storage::internal::Base64Decode(data.key);
    if (!key_bytes) return std::move(key_bytes).status();
    auto key_sha256_bytes = storage::internal::Base64Decode(data.sha256);
    if (!key_sha256_bytes) return std::move(key_sha256_bytes).status();

    result.set_copy_source_encryption_algorithm(data.algorithm);
    result.set_copy_source_encryption_key_bytes(
        std::string{key_bytes->begin(), key_bytes->end()});
    result.set_copy_source_encryption_key_sha256_bytes(
        std::string{key_sha256_bytes->begin(), key_sha256_bytes->end()});
  }
  return result;
}

StatusOr<google::storage::v2::StartResumableWriteRequest> ToProto(
    storage::internal::ResumableUploadRequest const& request) {
  google::storage::v2::StartResumableWriteRequest result;
  auto status = SetCommonObjectParameters(result, request);
  if (!status.ok()) return status;

  auto& object_spec = *result.mutable_write_object_spec();
  auto& resource = *object_spec.mutable_resource();
  SetResourceOptions(resource, request);
  status = SetObjectMetadata(resource, request);
  if (!status.ok()) return status;
  SetStorageClass(resource, request);
  SetPredefinedAcl(object_spec, request);
  SetGenerationConditions(object_spec, request);
  SetMetagenerationConditions(object_spec, request);
  if (request.HasOption<storage::UploadContentLength>()) {
    object_spec.set_object_size(static_cast<std::int64_t>(
        request.GetOption<storage::UploadContentLength>().value()));
  }

  resource.set_bucket(GrpcBucketIdToName(request.bucket_name()));
  resource.set_name(request.object_name());

  return result;
}

google::storage::v2::QueryWriteStatusRequest ToProto(
    storage::internal::QueryResumableUploadRequest const& request) {
  google::storage::v2::QueryWriteStatusRequest r;
  r.set_upload_id(request.upload_session_url());
  return r;
}

storage::internal::QueryResumableUploadResponse FromProto(
    google::storage::v2::QueryWriteStatusResponse const& response,
    Options const& options) {
  storage::internal::QueryResumableUploadResponse result;
  if (response.has_persisted_size()) {
    result.committed_size =
        static_cast<std::uint64_t>(response.persisted_size());
  }
  if (response.has_resource()) {
    result.payload = storage_internal::FromProto(response.resource(), options);
  }
  return result;
}

google::storage::v2::CancelResumableWriteRequest ToProto(
    storage::internal::DeleteResumableUploadRequest const& request) {
  google::storage::v2::CancelResumableWriteRequest result;
  result.set_upload_id(request.upload_session_url());
  return result;
}

Status Finalize(google::storage::v2::WriteObjectRequest& write_request,
                grpc::WriteOptions& options,
                storage::internal::HashFunction& hash_function,
                storage::internal::HashValues hashes) {
  write_request.set_finish_write(true);
  options.set_last_message();
  return FinalizeChecksums(*write_request.mutable_object_checksums(),
                           Merge(std::move(hashes), hash_function.Finish()));
}

Status Finalize(google::storage::v2::BidiWriteObjectRequest& write_request,
                grpc::WriteOptions& options,
                storage::internal::HashFunction& hash_function,
                storage::internal::HashValues hashes) {
  write_request.set_finish_write(true);
  options.set_last_message();
  return FinalizeChecksums(*write_request.mutable_object_checksums(),
                           Merge(std::move(hashes), hash_function.Finish()));
}

// If this is the last `Write()` call of the last `InsertObjectMedia()` set the
// flags to finalize the request
Status MaybeFinalize(google::storage::v2::WriteObjectRequest& write_request,
                     grpc::WriteOptions& options,
                     storage::internal::InsertObjectMediaRequest const& request,
                     bool chunk_has_more) {
  if (chunk_has_more) return {};
  return Finalize(write_request, options, request.hash_function());
}

// If this is the last `Write()` call of the last `UploadChunk()` set the flags
// to finalize the request
Status MaybeFinalize(google::storage::v2::WriteObjectRequest& write_request,
                     grpc::WriteOptions& options,
                     storage::internal::UploadChunkRequest const& request,
                     bool chunk_has_more) {
  if (!chunk_has_more) options.set_last_message();
  if (!request.last_chunk() || chunk_has_more) return {};
  return Finalize(write_request, options, request.hash_function(),
                  request.known_object_hashes());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

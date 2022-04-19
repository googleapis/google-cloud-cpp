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

#include "google/cloud/storage/internal/grpc_object_request_parser.h"
#include "google/cloud/storage/internal/grpc_common_request_params.h"
#include "google/cloud/storage/internal/grpc_object_access_control_parser.h"
#include "google/cloud/storage/internal/grpc_object_metadata_parser.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/internal/patch_builder_details.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/log.h"
#include <crc32c/crc32c.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {
template <typename GrpcRequest, typename StorageRequest>
Status SetCommonObjectParameters(GrpcRequest& request,
                                 StorageRequest const& req) {
  if (req.template HasOption<EncryptionKey>()) {
    auto data = req.template GetOption<EncryptionKey>().value();
    auto key_bytes = Base64Decode(data.key);
    if (!key_bytes) return std::move(key_bytes).status();
    auto key_sha256_bytes = Base64Decode(data.sha256);
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
    typename std::enable_if<
        std::is_same<std::string const&,
                     google::cloud::internal::invoke_result_t<
                         GetPredefinedAcl<GrpcRequest>, GrpcRequest>>::value,
        int>::type = 0>
void SetPredefinedAcl(GrpcRequest& request, StorageRequest const& req) {
  if (req.template HasOption<PredefinedAcl>()) {
    request.set_predefined_acl(req.template GetOption<PredefinedAcl>().value());
  }
}

template <typename GrpcRequest, typename StorageRequest>
void SetMetagenerationConditions(GrpcRequest& request,
                                 StorageRequest const& req) {
  if (req.template HasOption<IfMetagenerationMatch>()) {
    request.set_if_metageneration_match(
        req.template GetOption<IfMetagenerationMatch>().value());
  }
  if (req.template HasOption<IfMetagenerationNotMatch>()) {
    request.set_if_metageneration_not_match(
        req.template GetOption<IfMetagenerationNotMatch>().value());
  }
}

template <typename GrpcRequest, typename StorageRequest>
void SetGenerationConditions(GrpcRequest& request, StorageRequest const& req) {
  if (req.template HasOption<IfGenerationMatch>()) {
    request.set_if_generation_match(
        req.template GetOption<IfGenerationMatch>().value());
  }
  if (req.template HasOption<IfGenerationNotMatch>()) {
    request.set_if_generation_not_match(
        req.template GetOption<IfGenerationNotMatch>().value());
  }
}

template <typename StorageRequest>
void SetResourceOptions(google::storage::v2::Object& resource,
                        StorageRequest const& request) {
  if (request.template HasOption<ContentEncoding>()) {
    resource.set_content_encoding(
        request.template GetOption<ContentEncoding>().value());
  }
  if (request.template HasOption<ContentType>()) {
    resource.set_content_type(
        request.template GetOption<ContentType>().value());
  }
  if (request.template HasOption<KmsKeyName>()) {
    resource.set_kms_key(request.template GetOption<KmsKeyName>().value());
  }
}

template <typename StorageRequest>
Status SetObjectMetadata(google::storage::v2::Object& resource,
                         StorageRequest const& req) {
  if (!req.template HasOption<WithObjectMetadata>()) {
    return Status{};
  }
  auto metadata = req.template GetOption<WithObjectMetadata>().value();
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
    *resource.add_acl() = GrpcObjectAccessControlParser::ToProto(acl);
  }
  if (!metadata.content_language().empty()) {
    resource.set_content_language(metadata.content_language());
  }
  if (!metadata.content_type().empty()) {
    resource.set_content_type(metadata.content_type());
  }
  if (metadata.event_based_hold()) {
    resource.set_event_based_hold(metadata.event_based_hold());
  }

  for (auto const& kv : metadata.metadata()) {
    (*resource.mutable_metadata())[kv.first] = kv.second;
  }

  if (!metadata.storage_class().empty()) {
    resource.set_storage_class(metadata.storage_class());
  }
  resource.set_temporary_hold(metadata.temporary_hold());

  if (metadata.has_customer_encryption()) {
    auto encryption =
        GrpcObjectMetadataParser::ToProto(metadata.customer_encryption());
    if (!encryption) return std::move(encryption).status();
    *resource.mutable_customer_encryption() = *std::move(encryption);
  }
  return Status{};
}

}  // namespace

StatusOr<google::storage::v2::ComposeObjectRequest>
GrpcObjectRequestParser::ToProto(ComposeObjectRequest const& request) {
  google::storage::v2::ComposeObjectRequest result;
  auto status = SetCommonObjectParameters(result, request);
  if (!status.ok()) return status;
  SetCommonParameters(result, request);

  auto& destination = *result.mutable_destination();
  destination.set_bucket("projects/_/buckets/" + request.bucket_name());
  destination.set_name(request.object_name());
  if (request.HasOption<WithObjectMetadata>()) {
    auto metadata = request.GetOption<WithObjectMetadata>().value();
    for (auto const& a : metadata.acl()) {
      *destination.add_acl() = GrpcObjectAccessControlParser::ToProto(a);
    }
    for (auto const& kv : metadata.metadata()) {
      (*destination.mutable_metadata())[kv.first] = kv.second;
    }
    destination.set_content_encoding(metadata.content_encoding());
    destination.set_content_disposition(metadata.content_disposition());
    destination.set_cache_control(metadata.cache_control());
    destination.set_content_language(metadata.content_language());
    destination.set_content_type(metadata.content_type());
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
  if (request.HasOption<DestinationPredefinedAcl>()) {
    result.set_destination_predefined_acl(
        request.GetOption<DestinationPredefinedAcl>().value());
  }
  if (request.HasOption<IfGenerationMatch>()) {
    result.set_if_generation_match(
        request.GetOption<IfGenerationMatch>().value());
  }
  if (request.HasOption<IfMetagenerationMatch>()) {
    result.set_if_metageneration_match(
        request.GetOption<IfMetagenerationMatch>().value());
  }
  result.set_kms_key(request.GetOption<KmsKeyName>().value_or(""));
  return result;
}

google::storage::v2::DeleteObjectRequest GrpcObjectRequestParser::ToProto(
    DeleteObjectRequest const& request) {
  google::storage::v2::DeleteObjectRequest result;
  SetGenerationConditions(result, request);
  SetMetagenerationConditions(result, request);
  SetCommonParameters(result, request);
  result.set_bucket("projects/_/buckets/" + request.bucket_name());
  result.set_object(request.object_name());
  result.set_generation(request.GetOption<Generation>().value_or(0));
  return result;
}

google::storage::v2::GetObjectRequest GrpcObjectRequestParser::ToProto(
    GetObjectMetadataRequest const& request) {
  google::storage::v2::GetObjectRequest result;
  SetGenerationConditions(result, request);
  SetMetagenerationConditions(result, request);
  SetCommonParameters(result, request);

  result.set_bucket("projects/_/buckets/" + request.bucket_name());
  result.set_object(request.object_name());
  result.set_generation(request.GetOption<Generation>().value_or(0));
  auto projection = request.GetOption<Projection>().value_or("");
  if (projection == "full") result.mutable_read_mask()->add_paths("*");
  return result;
}

StatusOr<google::storage::v2::ReadObjectRequest>
GrpcObjectRequestParser::ToProto(ReadObjectRangeRequest const& request) {
  google::storage::v2::ReadObjectRequest r;
  auto status = SetCommonObjectParameters(r, request);
  if (!status.ok()) return status;
  r.set_object(request.object_name());
  r.set_bucket("projects/_/buckets/" + request.bucket_name());
  if (request.HasOption<Generation>()) {
    r.set_generation(request.GetOption<Generation>().value());
  }
  if (request.HasOption<ReadRange>()) {
    auto const range = request.GetOption<ReadRange>().value();
    r.set_read_offset(range.begin);
    r.set_read_limit(range.end - range.begin);
  }
  if (request.HasOption<ReadLast>()) {
    auto const offset = request.GetOption<ReadLast>().value();
    r.set_read_offset(-offset);
  }
  if (request.HasOption<ReadFromOffset>()) {
    auto const offset = request.GetOption<ReadFromOffset>().value();
    if (offset > r.read_offset()) {
      if (r.read_limit() > 0) {
        r.set_read_limit(offset - r.read_offset());
      }
      r.set_read_offset(offset);
    }
  }
  SetGenerationConditions(r, request);
  SetMetagenerationConditions(r, request);
  SetCommonParameters(r, request);

  return r;
}

StatusOr<google::storage::v2::UpdateObjectRequest>
GrpcObjectRequestParser::ToProto(PatchObjectRequest const& request) {
  google::storage::v2::UpdateObjectRequest result;
  auto status = SetCommonObjectParameters(result, request);
  if (!status.ok()) return status;
  SetGenerationConditions(result, request);
  SetMetagenerationConditions(result, request);
  SetCommonParameters(result, request);
  SetPredefinedAcl(result, request);

  auto& object = *result.mutable_object();
  object.set_bucket("projects/_/buckets/" + request.bucket_name());
  object.set_name(request.object_name());
  object.set_generation(request.GetOption<Generation>().value_or(0));

  auto const& patch = PatchBuilderDetails::GetPatch(request.patch().impl_);

  if (patch.contains("acl")) {
    for (auto const& a : patch["acl"]) {
      auto acl = ObjectAccessControlParser::FromJson(a);
      if (!acl) return std::move(acl).status();
      *object.add_acl() = GrpcObjectAccessControlParser::ToProto(*acl);
    }
    result.mutable_update_mask()->add_paths("acl");
  }

  if (request.patch().metadata_subpatch_dirty_) {
    auto const& subpatch =
        PatchBuilderDetails::GetPatch(request.patch().metadata_subpatch_);
    // The semantics in gRPC are to replace any metadata attributes
    result.mutable_update_mask()->add_paths("metadata");
    for (auto const& kv : subpatch.items()) {
      auto const& v = kv.value();
      if (!v.is_string()) continue;
      (*object.mutable_metadata())[kv.key()] = v.get<std::string>();
    }
  }

  if (patch.contains("customTime")) {
    auto ts =
        google::cloud::internal::ParseRfc3339(patch.value("customTime", ""));
    if (!ts) return std::move(ts).status();
    result.mutable_update_mask()->add_paths("custom_time");
    *object.mutable_custom_time() =
        google::cloud::internal::ToProtoTimestamp(*ts);
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
    f.setter(patch.value(f.json_name, ""));
    result.mutable_update_mask()->add_paths(f.grpc_name);
  }

  if (patch.contains("eventBasedHold")) {
    object.set_event_based_hold(patch.value("eventBasedHold", false));
    result.mutable_update_mask()->add_paths("event_based_hold");
  }
  if (patch.contains("temporaryHold")) {
    object.set_temporary_hold(patch.value("temporaryHold", false));
    result.mutable_update_mask()->add_paths("temporary_hold");
  }

  return result;
}

StatusOr<google::storage::v2::UpdateObjectRequest>
GrpcObjectRequestParser::ToProto(UpdateObjectRequest const& request) {
  google::storage::v2::UpdateObjectRequest result;
  auto status = SetCommonObjectParameters(result, request);
  if (!status.ok()) return status;
  SetGenerationConditions(result, request);
  SetMetagenerationConditions(result, request);
  SetCommonParameters(result, request);
  SetPredefinedAcl(result, request);

  auto& object = *result.mutable_object();
  object.set_bucket("projects/_/buckets/" + request.bucket_name());
  object.set_name(request.object_name());
  object.set_generation(request.GetOption<Generation>().value_or(0));

  result.mutable_update_mask()->add_paths("acl");
  for (auto const& a : request.metadata().acl()) {
    *object.add_acl() = GrpcObjectAccessControlParser::ToProto(a);
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

StatusOr<google::storage::v2::WriteObjectRequest>
GrpcObjectRequestParser::ToProto(InsertObjectMediaRequest const& request) {
  google::storage::v2::WriteObjectRequest r;
  auto& object_spec = *r.mutable_write_object_spec();
  auto& resource = *object_spec.mutable_resource();
  SetResourceOptions(resource, request);
  auto status = SetObjectMetadata(resource, request);
  if (!status.ok()) return status;
  SetPredefinedAcl(object_spec, request);
  SetGenerationConditions(object_spec, request);
  SetMetagenerationConditions(object_spec, request);
  status = SetCommonObjectParameters(r, request);
  if (!status.ok()) return status;
  SetCommonParameters(r, request);

  resource.set_bucket("projects/_/buckets/" + request.bucket_name());
  resource.set_name(request.object_name());
  r.set_write_offset(0);

  auto& checksums = *r.mutable_object_checksums();
  if (request.HasOption<Crc32cChecksumValue>()) {
    // The client library accepts CRC32C checksums in the format required by the
    // REST APIs (base64-encoded big-endian, 32-bit integers). We need to
    // convert this to the format expected by proto, which is just a 32-bit
    // integer. But the value received by the application might be incorrect, so
    // we need to validate it.
    auto as_proto = GrpcObjectMetadataParser::Crc32cToProto(
        request.GetOption<Crc32cChecksumValue>().value());
    if (!as_proto.ok()) return std::move(as_proto).status();
    checksums.set_crc32c(*as_proto);
  } else if (request.GetOption<DisableCrc32cChecksum>().value_or(false)) {
    // Nothing to do, the option is disabled (mostly useful in tests).
  } else {
    checksums.set_crc32c(crc32c::Crc32c(request.contents()));
  }

  if (request.HasOption<MD5HashValue>()) {
    auto as_proto = GrpcObjectMetadataParser::MD5ToProto(
        request.GetOption<MD5HashValue>().value());
    if (!as_proto.ok()) return std::move(as_proto).status();
    checksums.set_md5_hash(*std::move(as_proto));
  } else if (request.GetOption<DisableMD5Hash>().value_or(false)) {
    // Nothing to do, the option is disabled.
  } else {
    checksums.set_md5_hash(
        GrpcObjectMetadataParser::ComputeMD5Hash(request.contents()));
  }

  return r;
}

QueryResumableUploadResponse GrpcObjectRequestParser::FromProto(
    google::storage::v2::WriteObjectResponse const& p, Options const& options) {
  QueryResumableUploadResponse response;
  if (p.has_persisted_size()) {
    response.committed_size = static_cast<std::uint64_t>(p.persisted_size());
  }
  if (p.has_resource()) {
    response.payload =
        GrpcObjectMetadataParser::FromProto(p.resource(), options);
  }
  return response;
}

google::storage::v2::ListObjectsRequest GrpcObjectRequestParser::ToProto(
    ListObjectsRequest const& request) {
  google::storage::v2::ListObjectsRequest result;
  result.set_parent("projects/_/buckets/" + request.bucket_name());
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
  result.set_delimiter(request.GetOption<Delimiter>().value_or(""));
  result.set_include_trailing_delimiter(
      request.GetOption<IncludeTrailingDelimiter>().value_or(false));
  result.set_prefix(request.GetOption<Prefix>().value_or(""));
  result.set_versions(request.GetOption<Versions>().value_or(""));
  result.set_lexicographic_start(request.GetOption<StartOffset>().value_or(""));
  result.set_lexicographic_end(request.GetOption<EndOffset>().value_or(""));
  SetCommonParameters(result, request);
  return result;
}

ListObjectsResponse GrpcObjectRequestParser::FromProto(
    google::storage::v2::ListObjectsResponse const& response,
    Options const& options) {
  ListObjectsResponse result;
  result.next_page_token = response.next_page_token();
  for (auto const& o : response.objects()) {
    result.items.push_back(GrpcObjectMetadataParser::FromProto(o, options));
  }
  for (auto const& p : response.prefixes()) result.prefixes.push_back(p);
  return result;
}

StatusOr<google::storage::v2::RewriteObjectRequest>
GrpcObjectRequestParser::ToProto(RewriteObjectRequest const& request) {
  google::storage::v2::RewriteObjectRequest result;
  SetCommonParameters(result, request);
  auto status = SetCommonObjectParameters(result, request);
  if (!status.ok()) return status;

  result.set_destination_name(request.destination_object());
  result.set_destination_bucket("projects/_/buckets/" +
                                request.destination_bucket());

  if (request.HasOption<WithObjectMetadata>() ||
      request.HasOption<DestinationKmsKeyName>()) {
    auto& destination = *result.mutable_destination();
    destination.set_kms_key(
        request.GetOption<DestinationKmsKeyName>().value_or(""));
    // Only a few fields can be set as part of the metadata request.
    auto m = request.GetOption<WithObjectMetadata>().value();
    destination.set_storage_class(m.storage_class());
    destination.set_content_encoding(m.content_encoding());
    destination.set_content_disposition(m.content_disposition());
    destination.set_cache_control(m.cache_control());
    destination.set_content_language(m.content_language());
    destination.set_content_type(m.content_type());
    destination.set_temporary_hold(m.temporary_hold());
    for (auto const& kv : m.metadata()) {
      (*destination.mutable_metadata())[kv.first] = kv.second;
    }
    if (m.event_based_hold()) {
      // The proto is an optional<bool>, avoid setting it to `false`, seems
      // confusing.
      destination.set_event_based_hold(m.event_based_hold());
    }
    if (m.has_custom_time()) {
      *destination.mutable_custom_time() =
          google::cloud::internal::ToProtoTimestamp(m.custom_time());
    }
  }
  result.set_source_bucket("projects/_/buckets/" + request.source_bucket());
  result.set_source_object(request.source_object());
  result.set_source_generation(
      request.GetOption<SourceGeneration>().value_or(0));
  result.set_rewrite_token(request.rewrite_token());
  if (request.HasOption<DestinationPredefinedAcl>()) {
    result.set_destination_predefined_acl(
        request.GetOption<DestinationPredefinedAcl>().value());
  }
  SetGenerationConditions(result, request);
  SetMetagenerationConditions(result, request);
  if (request.HasOption<IfSourceGenerationMatch>()) {
    result.set_if_source_generation_match(
        request.GetOption<IfSourceGenerationMatch>().value());
  }
  if (request.HasOption<IfSourceGenerationNotMatch>()) {
    result.set_if_source_generation_not_match(
        request.GetOption<IfSourceGenerationNotMatch>().value());
  }
  if (request.HasOption<IfSourceMetagenerationMatch>()) {
    result.set_if_source_metageneration_match(
        request.GetOption<IfSourceMetagenerationMatch>().value());
  }
  if (request.HasOption<IfSourceMetagenerationNotMatch>()) {
    result.set_if_source_metageneration_not_match(
        request.GetOption<IfSourceMetagenerationNotMatch>().value());
  }
  result.set_max_bytes_rewritten_per_call(
      request.GetOption<MaxBytesRewrittenPerCall>().value_or(0));
  if (request.HasOption<SourceEncryptionKey>()) {
    auto data = request.template GetOption<SourceEncryptionKey>().value();
    auto key_bytes = Base64Decode(data.key);
    if (!key_bytes) return std::move(key_bytes).status();
    auto key_sha256_bytes = Base64Decode(data.sha256);
    if (!key_sha256_bytes) return std::move(key_sha256_bytes).status();

    result.set_copy_source_encryption_algorithm(data.algorithm);
    result.set_copy_source_encryption_key_bytes(
        std::string{key_bytes->begin(), key_bytes->end()});
    result.set_copy_source_encryption_key_sha256_bytes(
        std::string{key_sha256_bytes->begin(), key_sha256_bytes->end()});
  }
  return result;
}

RewriteObjectResponse GrpcObjectRequestParser::FromProto(
    google::storage::v2::RewriteResponse const& response,
    Options const& options) {
  RewriteObjectResponse result;
  result.done = response.done();
  result.object_size = response.object_size();
  result.total_bytes_rewritten = response.total_bytes_rewritten();
  result.rewrite_token = response.rewrite_token();
  if (response.has_resource()) {
    result.resource =
        GrpcObjectMetadataParser::FromProto(response.resource(), options);
  }
  return result;
}

StatusOr<google::storage::v2::RewriteObjectRequest>
GrpcObjectRequestParser::ToProto(CopyObjectRequest const& request) {
  google::storage::v2::RewriteObjectRequest result;
  SetCommonParameters(result, request);
  auto status = SetCommonObjectParameters(result, request);
  if (!status.ok()) return status;

  result.set_destination_name(request.destination_object());
  result.set_destination_bucket("projects/_/buckets/" +
                                request.destination_bucket());

  if (request.HasOption<WithObjectMetadata>() ||
      request.HasOption<DestinationKmsKeyName>()) {
    auto& destination = *result.mutable_destination();
    destination.set_kms_key(
        request.GetOption<DestinationKmsKeyName>().value_or(""));
    // Only a few fields can be set as part of the metadata request.
    auto m = request.GetOption<WithObjectMetadata>().value();
    destination.set_storage_class(m.storage_class());
    destination.set_content_encoding(m.content_encoding());
    destination.set_content_disposition(m.content_disposition());
    destination.set_cache_control(m.cache_control());
    destination.set_content_language(m.content_language());
    destination.set_content_type(m.content_type());
    destination.set_temporary_hold(m.temporary_hold());
    for (auto const& kv : m.metadata()) {
      (*destination.mutable_metadata())[kv.first] = kv.second;
    }
    if (m.event_based_hold()) {
      // The proto is an optional<bool>, avoid setting it to `false`, seems
      // confusing.
      destination.set_event_based_hold(m.event_based_hold());
    }
    if (m.has_custom_time()) {
      *destination.mutable_custom_time() =
          google::cloud::internal::ToProtoTimestamp(m.custom_time());
    }
  }
  result.set_source_bucket("projects/_/buckets/" + request.source_bucket());
  result.set_source_object(request.source_object());
  result.set_source_generation(
      request.GetOption<SourceGeneration>().value_or(0));
  if (request.HasOption<DestinationPredefinedAcl>()) {
    result.set_destination_predefined_acl(
        request.GetOption<DestinationPredefinedAcl>().value());
  }
  SetGenerationConditions(result, request);
  SetMetagenerationConditions(result, request);
  if (request.HasOption<IfSourceGenerationMatch>()) {
    result.set_if_source_generation_match(
        request.GetOption<IfSourceGenerationMatch>().value());
  }
  if (request.HasOption<IfSourceGenerationNotMatch>()) {
    result.set_if_source_generation_not_match(
        request.GetOption<IfSourceGenerationNotMatch>().value());
  }
  if (request.HasOption<IfSourceMetagenerationMatch>()) {
    result.set_if_source_metageneration_match(
        request.GetOption<IfSourceMetagenerationMatch>().value());
  }
  if (request.HasOption<IfSourceMetagenerationNotMatch>()) {
    result.set_if_source_metageneration_not_match(
        request.GetOption<IfSourceMetagenerationNotMatch>().value());
  }
  if (request.HasOption<SourceEncryptionKey>()) {
    auto data = request.template GetOption<SourceEncryptionKey>().value();
    auto key_bytes = Base64Decode(data.key);
    if (!key_bytes) return std::move(key_bytes).status();
    auto key_sha256_bytes = Base64Decode(data.sha256);
    if (!key_sha256_bytes) return std::move(key_sha256_bytes).status();

    result.set_copy_source_encryption_algorithm(data.algorithm);
    result.set_copy_source_encryption_key_bytes(
        std::string{key_bytes->begin(), key_bytes->end()});
    result.set_copy_source_encryption_key_sha256_bytes(
        std::string{key_sha256_bytes->begin(), key_sha256_bytes->end()});
  }
  return result;
}

StatusOr<google::storage::v2::StartResumableWriteRequest>
GrpcObjectRequestParser::ToProto(ResumableUploadRequest const& request) {
  google::storage::v2::StartResumableWriteRequest result;
  auto status = SetCommonObjectParameters(result, request);
  if (!status.ok()) return status;

  auto& object_spec = *result.mutable_write_object_spec();
  auto& resource = *object_spec.mutable_resource();
  SetResourceOptions(resource, request);
  status = SetObjectMetadata(resource, request);
  if (!status.ok()) return status;
  SetPredefinedAcl(object_spec, request);
  SetGenerationConditions(object_spec, request);
  SetMetagenerationConditions(object_spec, request);
  SetCommonParameters(result, request);

  resource.set_bucket("projects/_/buckets/" + request.bucket_name());
  resource.set_name(request.object_name());

  return result;
}

google::storage::v2::QueryWriteStatusRequest GrpcObjectRequestParser::ToProto(
    QueryResumableUploadRequest const& request) {
  google::storage::v2::QueryWriteStatusRequest r;
  r.set_upload_id(request.upload_session_url());
  return r;
}

QueryResumableUploadResponse GrpcObjectRequestParser::FromProto(
    google::storage::v2::QueryWriteStatusResponse const& response,
    Options const& options) {
  QueryResumableUploadResponse result;
  if (response.has_persisted_size()) {
    result.committed_size =
        static_cast<std::uint64_t>(response.persisted_size());
  }
  if (response.has_resource()) {
    result.payload =
        GrpcObjectMetadataParser::FromProto(response.resource(), options);
  }
  return result;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

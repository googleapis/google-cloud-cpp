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
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/internal/invoke_result.h"
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
        std::is_same<google::storage::v2::PredefinedObjectAcl,
                     google::cloud::internal::invoke_result_t<
                         GetPredefinedAcl<GrpcRequest>, GrpcRequest>>::value,
        int>::type = 0>
void SetPredefinedAcl(GrpcRequest& request, StorageRequest const& req) {
  if (req.template HasOption<PredefinedAcl>()) {
    request.set_predefined_acl(GrpcObjectRequestParser::ToProtoObject(
        req.template GetOption<PredefinedAcl>()));
  }
}

template <typename GrpcRequest, typename StorageRequest>
void SetPredefinedDefaultObjectAcl(GrpcRequest& request,
                                   StorageRequest const& req) {
  if (req.template HasOption<PredefinedDefaultObjectAcl>()) {
    request.set_predefined_default_object_acl(
        ToProto(req.template GetOption<PredefinedDefaultObjectAcl>()));
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

google::storage::v2::PredefinedObjectAcl GrpcObjectRequestParser::ToProtoObject(
    PredefinedAcl const& acl) {
  if (acl.value() == PredefinedAcl::BucketOwnerFullControl().value()) {
    return google::storage::v2::OBJECT_ACL_BUCKET_OWNER_FULL_CONTROL;
  }
  if (acl.value() == PredefinedAcl::BucketOwnerRead().value()) {
    return google::storage::v2::OBJECT_ACL_BUCKET_OWNER_READ;
  }
  if (acl.value() == PredefinedAcl::AuthenticatedRead().value()) {
    return google::storage::v2::OBJECT_ACL_AUTHENTICATED_READ;
  }
  if (acl.value() == PredefinedAcl::Private().value()) {
    return google::storage::v2::OBJECT_ACL_PRIVATE;
  }
  if (acl.value() == PredefinedAcl::ProjectPrivate().value()) {
    return google::storage::v2::OBJECT_ACL_PROJECT_PRIVATE;
  }
  if (acl.value() == PredefinedAcl::PublicRead().value()) {
    return google::storage::v2::OBJECT_ACL_PUBLIC_READ;
  }
  if (acl.value() == PredefinedAcl::PublicReadWrite().value()) {
    GCP_LOG(ERROR) << "Invalid predefinedAcl value " << acl;
    return google::storage::v2::PREDEFINED_OBJECT_ACL_UNSPECIFIED;
  }
  GCP_LOG(ERROR) << "Unknown predefinedAcl value " << acl;
  return google::storage::v2::PREDEFINED_OBJECT_ACL_UNSPECIFIED;
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

ResumableUploadResponse GrpcObjectRequestParser::FromProto(
    google::storage::v2::WriteObjectResponse const& p, Options const& options) {
  ResumableUploadResponse response;
  response.upload_state = ResumableUploadResponse::kInProgress;
  if (p.has_persisted_size()) {
    response.committed_size = static_cast<std::uint64_t>(p.persisted_size());
  }
  if (p.has_resource()) {
    response.payload =
        GrpcObjectMetadataParser::FromProto(p.resource(), options);
    response.upload_state = ResumableUploadResponse::kDone;
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

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

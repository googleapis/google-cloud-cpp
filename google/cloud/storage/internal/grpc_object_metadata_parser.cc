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

#include "google/cloud/storage/internal/grpc_object_metadata_parser.h"
#include "google/cloud/storage/internal/grpc_object_access_control_parser.h"
#include "google/cloud/storage/internal/grpc_owner_parser.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/big_endian.h"
#include "google/cloud/internal/time_utils.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

CustomerEncryption GrpcObjectMetadataParser::FromProto(
    google::storage::v2::CustomerEncryption rhs) {
  CustomerEncryption result;
  result.encryption_algorithm = std::move(*rhs.mutable_encryption_algorithm());
  result.key_sha256 = Base64Encode(rhs.key_sha256_bytes());
  return result;
}

StatusOr<google::storage::v2::CustomerEncryption>
GrpcObjectMetadataParser::ToProto(CustomerEncryption rhs) {
  auto key_sha256 = Base64Decode(rhs.key_sha256);
  if (!key_sha256) return std::move(key_sha256).status();
  google::storage::v2::CustomerEncryption result;
  result.set_encryption_algorithm(std::move(rhs.encryption_algorithm));
  result.set_key_sha256_bytes(
      std::string(key_sha256->begin(), key_sha256->end()));
  return result;
}

std::string GrpcObjectMetadataParser::Crc32cFromProto(std::uint32_t v) {
  auto endian_encoded = google::cloud::internal::EncodeBigEndian(v);
  return Base64Encode(endian_encoded);
}

StatusOr<std::uint32_t> GrpcObjectMetadataParser::Crc32cToProto(
    std::string const& v) {
  auto decoded = Base64Decode(v);
  if (!decoded) return std::move(decoded).status();
  return google::cloud::internal::DecodeBigEndian<std::uint32_t>(
      std::string(decoded->begin(), decoded->end()));
}

std::string GrpcObjectMetadataParser::MD5FromProto(std::string const& v) {
  return internal::Base64Encode(v);
}

StatusOr<std::string> GrpcObjectMetadataParser::MD5ToProto(
    std::string const& v) {
  if (v.empty()) return {};
  auto binary = internal::Base64Decode(v);
  if (!binary) return std::move(binary).status();
  return std::string{binary->begin(), binary->end()};
}

std::string GrpcObjectMetadataParser::ComputeMD5Hash(
    std::string const& payload) {
  auto b = internal::MD5Hash(payload);
  return std::string{b.begin(), b.end()};
}

ObjectMetadata GrpcObjectMetadataParser::FromProto(
    google::storage::v2::Object object, Options const& options) {
  auto bucket_id = [](google::storage::v2::Object const& object) {
    auto const& bucket_name = object.bucket();
    auto const pos = bucket_name.find_last_of('/');
    if (pos == std::string::npos) return bucket_name;
    return bucket_name.substr(pos + 1);
  };

  ObjectMetadata metadata;
  metadata.kind_ = "storage#object";
  metadata.bucket_ = bucket_id(object);
  metadata.name_ = std::move(*object.mutable_name());
  metadata.generation_ = object.generation();
  metadata.etag_ = object.etag();
  metadata.id_ = metadata.bucket() + "/" + metadata.name() + "/" +
                 std::to_string(metadata.generation());
  auto const metadata_endpoint = [&options]() -> std::string {
    if (options.get<RestEndpointOption>() != "https://storage.googleapis.com") {
      return options.get<RestEndpointOption>();
    }
    return "https://www.googleapis.com";
  }();
  auto const path = [&options]() -> std::string {
    if (!options.has<TargetApiVersionOption>()) return "/storage/v1";
    return "/storage/" + options.get<TargetApiVersionOption>();
  }();
  auto const rel_path = "/b/" + metadata.bucket() + "/o/" + metadata.name();
  metadata.self_link_ = metadata_endpoint + path + rel_path;
  metadata.media_link_ =
      options.get<RestEndpointOption>() + "/download" + path + rel_path +
      "?generation=" + std::to_string(metadata.generation()) + "&alt=media";

  metadata.metageneration_ = object.metageneration();
  if (object.has_owner()) {
    metadata.owner_ = storage_internal::FromProto(*object.mutable_owner());
  }
  metadata.storage_class_ = std::move(*object.mutable_storage_class());
  if (object.has_create_time()) {
    metadata.time_created_ =
        google::cloud::internal::ToChronoTimePoint(object.create_time());
  }
  if (object.has_update_time()) {
    metadata.updated_ =
        google::cloud::internal::ToChronoTimePoint(object.update_time());
  }
  std::vector<ObjectAccessControl> acl;
  acl.reserve(object.acl_size());
  for (auto& item : *object.mutable_acl()) {
    acl.push_back(GrpcObjectAccessControlParser::FromProto(
        std::move(item), metadata.bucket(), metadata.name(),
        metadata.generation()));
  }
  metadata.acl_ = std::move(acl);
  metadata.cache_control_ = std::move(*object.mutable_cache_control());
  metadata.component_count_ = object.component_count();
  metadata.content_disposition_ =
      std::move(*object.mutable_content_disposition());
  metadata.content_encoding_ = std::move(*object.mutable_content_encoding());
  metadata.content_language_ = std::move(*object.mutable_content_language());
  metadata.content_type_ = std::move(*object.mutable_content_type());
  if (object.has_checksums()) {
    if (object.checksums().has_crc32c()) {
      metadata.crc32c_ = Crc32cFromProto(object.checksums().crc32c());
    }
    if (!object.checksums().md5_hash().empty()) {
      metadata.md5_hash_ = MD5FromProto(object.checksums().md5_hash());
    }
  }
  if (object.has_customer_encryption()) {
    metadata.customer_encryption_ =
        FromProto(std::move(*object.mutable_customer_encryption()));
  }
  if (object.has_event_based_hold()) {
    metadata.event_based_hold_ = object.event_based_hold();
  }
  metadata.kms_key_name_ = std::move(*object.mutable_kms_key());

  for (auto const& kv : object.metadata()) {
    metadata.metadata_[kv.first] = kv.second;
  }
  if (object.has_retention_expire_time()) {
    metadata.retention_expiration_time_ =
        google::cloud::internal::ToChronoTimePoint(
            object.retention_expire_time());
  }
  metadata.size_ = static_cast<std::uint64_t>(object.size());
  metadata.temporary_hold_ = object.temporary_hold();
  if (object.has_delete_time()) {
    metadata.time_deleted_ =
        google::cloud::internal::ToChronoTimePoint(object.delete_time());
  }
  if (object.has_update_storage_class_time()) {
    metadata.time_storage_class_updated_ =
        google::cloud::internal::ToChronoTimePoint(
            object.update_storage_class_time());
  }
  if (object.has_custom_time()) {
    metadata.custom_time_ =
        google::cloud::internal::ToChronoTimePoint(object.custom_time());
  }

  return metadata;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

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
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

storage::CustomerEncryption FromProto(
    google::storage::v2::CustomerEncryption rhs) {
  storage::CustomerEncryption result;
  result.encryption_algorithm = std::move(*rhs.mutable_encryption_algorithm());
  result.key_sha256 = storage::internal::Base64Encode(rhs.key_sha256_bytes());
  return result;
}

StatusOr<google::storage::v2::CustomerEncryption> ToProto(
    storage::CustomerEncryption rhs) {
  auto key_sha256 = storage::internal::Base64Decode(rhs.key_sha256);
  if (!key_sha256) return std::move(key_sha256).status();
  google::storage::v2::CustomerEncryption result;
  result.set_encryption_algorithm(std::move(rhs.encryption_algorithm));
  result.set_key_sha256_bytes(
      std::string(key_sha256->begin(), key_sha256->end()));
  return result;
}

std::string Crc32cFromProto(std::uint32_t v) {
  auto endian_encoded = google::cloud::internal::EncodeBigEndian(v);
  return storage::internal::Base64Encode(endian_encoded);
}

StatusOr<std::uint32_t> Crc32cToProto(std::string const& v) {
  auto decoded = storage::internal::Base64Decode(v);
  if (!decoded) return std::move(decoded).status();
  return google::cloud::internal::DecodeBigEndian<std::uint32_t>(
      std::string(decoded->begin(), decoded->end()));
}

std::string MD5FromProto(std::string const& v) {
  return storage::internal::Base64Encode(v);
}

StatusOr<std::string> MD5ToProto(std::string const& v) {
  if (v.empty()) return {};
  auto binary = storage::internal::Base64Decode(v);
  if (!binary) return std::move(binary).status();
  return std::string{binary->begin(), binary->end()};
}

std::string ComputeMD5Hash(std::string const& payload) {
  auto b = storage::internal::MD5Hash(payload);
  return std::string{b.begin(), b.end()};
}

storage::ObjectMetadata FromProto(google::storage::v2::Object object,
                                  Options const& options) {
  auto bucket_id = [](google::storage::v2::Object const& object) {
    auto const& bucket_name = object.bucket();
    auto const pos = bucket_name.find_last_of('/');
    if (pos == std::string::npos) return bucket_name;
    return bucket_name.substr(pos + 1);
  };

  storage::ObjectMetadata metadata;
  metadata.set_kind("storage#object");
  metadata.set_bucket(bucket_id(object));
  metadata.set_name(std::move(*object.mutable_name()));
  metadata.set_generation(object.generation());
  metadata.set_etag(object.etag());
  metadata.set_id(metadata.bucket() + "/" + metadata.name() + "/" +
                  std::to_string(metadata.generation()));
  auto const metadata_endpoint = [&options]() -> std::string {
    if (options.get<storage::RestEndpointOption>() !=
        "https://storage.googleapis.com") {
      return options.get<storage::RestEndpointOption>();
    }
    return "https://www.googleapis.com";
  }();
  auto const path = [&options]() -> std::string {
    if (!options.has<storage::internal::TargetApiVersionOption>())
      return "/storage/v1";
    return "/storage/" +
           options.get<storage::internal::TargetApiVersionOption>();
  }();
  auto const rel_path = "/b/" + metadata.bucket() + "/o/" + metadata.name();
  metadata.set_self_link(metadata_endpoint + path + rel_path);
  metadata.set_media_link(options.get<storage::RestEndpointOption>() +
                          "/download" + path + rel_path + "?generation=" +
                          std::to_string(metadata.generation()) + "&alt=media");

  metadata.set_metageneration(object.metageneration());
  if (object.has_owner()) {
    metadata.set_owner(FromProto(*object.mutable_owner()));
  }
  metadata.set_storage_class(std::move(*object.mutable_storage_class()));
  if (object.has_create_time()) {
    metadata.set_time_created(
        google::cloud::internal::ToChronoTimePoint(object.create_time()));
  }
  if (object.has_update_time()) {
    metadata.set_updated(
        google::cloud::internal::ToChronoTimePoint(object.update_time()));
  }
  std::vector<storage::ObjectAccessControl> acl;
  acl.reserve(object.acl_size());
  for (auto& item : *object.mutable_acl()) {
    acl.push_back(FromProto(std::move(item), metadata.bucket(), metadata.name(),
                            metadata.generation()));
  }
  metadata.set_acl(std::move(acl));
  metadata.set_cache_control(std::move(*object.mutable_cache_control()));
  metadata.set_component_count(object.component_count());
  metadata.set_content_disposition(
      std::move(*object.mutable_content_disposition()));
  metadata.set_content_encoding(std::move(*object.mutable_content_encoding()));
  metadata.set_content_language(std::move(*object.mutable_content_language()));
  metadata.set_content_type(std::move(*object.mutable_content_type()));
  if (object.has_checksums()) {
    if (object.checksums().has_crc32c()) {
      metadata.set_crc32c(Crc32cFromProto(object.checksums().crc32c()));
    }
    if (!object.checksums().md5_hash().empty()) {
      metadata.set_md5_hash(MD5FromProto(object.checksums().md5_hash()));
    }
  }
  if (object.has_customer_encryption()) {
    metadata.set_customer_encryption(
        FromProto(std::move(*object.mutable_customer_encryption())));
  }
  if (object.has_event_based_hold()) {
    metadata.set_event_based_hold(object.event_based_hold());
  }
  metadata.set_kms_key_name(std::move(*object.mutable_kms_key()));

  for (auto const& kv : object.metadata()) {
    metadata.upsert_metadata(kv.first, kv.second);
  }
  if (object.has_retention_expire_time()) {
    metadata.set_retention_expiration_time(
        google::cloud::internal::ToChronoTimePoint(
            object.retention_expire_time()));
  }
  metadata.set_size(static_cast<std::uint64_t>(object.size()));
  metadata.set_temporary_hold(object.temporary_hold());
  if (object.has_delete_time()) {
    metadata.set_time_deleted(
        google::cloud::internal::ToChronoTimePoint(object.delete_time()));
  }
  if (object.has_update_storage_class_time()) {
    metadata.set_time_storage_class_updated(
        google::cloud::internal::ToChronoTimePoint(
            object.update_storage_class_time()));
  }
  if (object.has_custom_time()) {
    metadata.set_custom_time(
        google::cloud::internal::ToChronoTimePoint(object.custom_time()));
  }

  return metadata;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

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

#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/internal/binary_data_as_debug_string.h"
#include "google/cloud/storage/internal/metadata_parser.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/internal/object_acl_requests.h"
#include "google/cloud/storage/object_metadata.h"
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
/**
 * Sets a string field in @p json when @p value is not empty.
 *
 * This simplifies the implementation of ToJsonString() because we repeat this
 * check for many attributes.
 */
void SetIfNotEmpty(internal::nl::json& json, char const* key,
                   std::string const& value) {
  if (value.empty()) {
    return;
  }
  json[key] = value;
}
}  // namespace

StatusOr<ObjectMetadata> ObjectMetadataParser::FromJson(
    internal::nl::json const& json) {
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  ObjectMetadata result{};
  auto status = CommonMetadata<ObjectMetadata>::ParseFromJson(result, json);
  if (!status.ok()) {
    return status;
  }

  if (json.count("acl") != 0) {
    for (auto const& kv : json["acl"].items()) {
      auto parsed = ObjectAccessControlParser::FromJson(kv.value());
      if (!parsed.ok()) {
        return std::move(parsed).status();
      }
      result.acl_.emplace_back(std::move(*parsed));
    }
  }

  result.bucket_ = json.value("bucket", "");
  result.cache_control_ = json.value("cacheControl", "");
  result.component_count_ = internal::ParseIntField(json, "componentCount");
  result.content_disposition_ = json.value("contentDisposition", "");
  result.content_encoding_ = json.value("contentEncoding", "");
  result.content_language_ = json.value("contentLanguage", "");
  result.content_type_ = json.value("contentType", "");
  result.crc32c_ = json.value("crc32c", "");
  if (json.count("customerEncryption") != 0) {
    auto field = json["customerEncryption"];
    CustomerEncryption e;
    e.encryption_algorithm = field.value("encryptionAlgorithm", "");
    e.key_sha256 = field.value("keySha256", "");
    result.customer_encryption_ = std::move(e);
  }
  result.event_based_hold_ = internal::ParseBoolField(json, "eventBasedHold");
  result.generation_ = internal::ParseLongField(json, "generation");
  result.kms_key_name_ = json.value("kmsKeyName", "");
  result.md5_hash_ = json.value("md5Hash", "");
  result.media_link_ = json.value("mediaLink", "");
  if (json.count("metadata") > 0) {
    for (auto const& kv : json["metadata"].items()) {
      result.metadata_.emplace(kv.key(), kv.value().get<std::string>());
    }
  }
  result.retention_expiration_time_ =
      internal::ParseTimestampField(json, "retentionExpirationTime");
  result.size_ = internal::ParseUnsignedLongField(json, "size");
  result.temporary_hold_ = internal::ParseBoolField(json, "temporaryHold");
  result.time_deleted_ = internal::ParseTimestampField(json, "timeDeleted");
  result.time_storage_class_updated_ =
      internal::ParseTimestampField(json, "timeStorageClassUpdated");
  return result;
}

StatusOr<ObjectMetadata> ObjectMetadataParser::FromString(
    std::string const& payload) {
  auto json = internal::nl::json::parse(payload, nullptr, false);
  return FromJson(json);
}

internal::nl::json ObjectMetadataJsonForCompose(ObjectMetadata const& meta) {
  using ::google::cloud::storage::internal::nl::json;
  json metadata_as_json({});
  if (!meta.acl().empty()) {
    for (ObjectAccessControl const& a : meta.acl()) {
      json entry;
      SetIfNotEmpty(entry, "entity", a.entity());
      SetIfNotEmpty(entry, "role", a.role());
      metadata_as_json["acl"].emplace_back(std::move(entry));
    }
  }

  SetIfNotEmpty(metadata_as_json, "cacheControl", meta.cache_control());
  SetIfNotEmpty(metadata_as_json, "contentDisposition",
                meta.content_disposition());
  SetIfNotEmpty(metadata_as_json, "contentEncoding", meta.content_encoding());
  SetIfNotEmpty(metadata_as_json, "contentLanguage", meta.content_language());
  SetIfNotEmpty(metadata_as_json, "contentType", meta.content_type());

  if (meta.event_based_hold()) {
    metadata_as_json["eventBasedHold"] = true;
  }

  SetIfNotEmpty(metadata_as_json, "name", meta.name());
  SetIfNotEmpty(metadata_as_json, "storageClass", meta.storage_class());

  if (!meta.metadata().empty()) {
    json meta_as_json;
    for (auto const& kv : meta.metadata()) {
      meta_as_json[kv.first] = kv.second;
    }
    metadata_as_json["metadata"] = std::move(meta_as_json);
  }

  return metadata_as_json;
}

internal::nl::json ObjectMetadataJsonForCopy(ObjectMetadata const& meta) {
  return ObjectMetadataJsonForCompose(meta);
}

internal::nl::json ObjectMetadataJsonForInsert(ObjectMetadata const& meta) {
  auto json = ObjectMetadataJsonForCompose(meta);
  SetIfNotEmpty(json, "crc32c", meta.crc32c());
  SetIfNotEmpty(json, "md5Hash", meta.md5_hash());
  return json;
}

internal::nl::json ObjectMetadataJsonForRewrite(ObjectMetadata const& meta) {
  return ObjectMetadataJsonForCompose(meta);
}

internal::nl::json ObjectMetadataJsonForUpdate(ObjectMetadata const& meta) {
  using ::google::cloud::storage::internal::nl::json;
  json metadata_as_json({});
  if (!meta.acl().empty()) {
    for (ObjectAccessControl const& a : meta.acl()) {
      json entry;
      SetIfNotEmpty(entry, "entity", a.entity());
      SetIfNotEmpty(entry, "role", a.role());
      metadata_as_json["acl"].emplace_back(std::move(entry));
    }
  }

  SetIfNotEmpty(metadata_as_json, "cacheControl", meta.cache_control());
  SetIfNotEmpty(metadata_as_json, "contentDisposition",
                meta.content_disposition());
  SetIfNotEmpty(metadata_as_json, "contentEncoding", meta.content_encoding());
  SetIfNotEmpty(metadata_as_json, "contentLanguage", meta.content_language());
  SetIfNotEmpty(metadata_as_json, "contentType", meta.content_type());

  metadata_as_json["eventBasedHold"] = meta.event_based_hold();

  if (!meta.metadata().empty()) {
    json meta_as_json;
    for (auto const& kv : meta.metadata()) {
      meta_as_json[kv.first] = kv.second;
    }
    metadata_as_json["metadata"] = std::move(meta_as_json);
  }

  return metadata_as_json;
}

std::ostream& operator<<(std::ostream& os, ListObjectsRequest const& r) {
  os << "ListObjectsRequest={bucket_name=" << r.bucket_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

StatusOr<ListObjectsResponse> ListObjectsResponse::FromHttpResponse(
    std::string const& payload) {
  auto json = storage::internal::nl::json::parse(payload, nullptr, false);
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }

  ListObjectsResponse result;
  result.next_page_token = json.value("nextPageToken", "");

  for (auto const& kv : json["items"].items()) {
    auto parsed = internal::ObjectMetadataParser::FromJson(kv.value());
    if (!parsed.ok()) {
      return std::move(parsed).status();
    }
    result.items.emplace_back(std::move(*parsed));
  }

  return result;
}

std::ostream& operator<<(std::ostream& os, ListObjectsResponse const& r) {
  os << "ListObjectsResponse={next_page_token=" << r.next_page_token
     << ", items={";
  std::copy(r.items.begin(), r.items.end(),
            std::ostream_iterator<ObjectMetadata>(os, "\n  "));
  return os << "}}";
}

std::ostream& operator<<(std::ostream& os, GetObjectMetadataRequest const& r) {
  os << "GetObjectMetadataRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, InsertObjectMediaRequest const& r) {
  os << "InsertObjectMediaRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name();
  r.DumpOptions(os, ", ");
  os << ", contents=\n"
     << BinaryDataAsDebugString(r.contents().data(), r.contents().size());
  return os << "}";
}

std::ostream& operator<<(std::ostream& os,
                         InsertObjectStreamingRequest const& r) {
  os << "InsertObjectStreamingRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, CopyObjectRequest const& r) {
  os << "CopyObjectRequest={destination_bucket=" << r.destination_bucket()
     << ", destination_object=" << r.destination_object()
     << ", source_bucket=" << r.source_bucket()
     << ", source_object=" << r.source_object();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, ReadObjectRangeRequest const& r) {
  os << "ReadObjectRangeRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

ReadObjectRangeResponse ReadObjectRangeResponse::FromHttpResponse(
    HttpResponse&& response) {
  auto loc = response.headers.find(std::string("content-range"));
  if (response.headers.end() == loc) {
    google::cloud::internal::ThrowInvalidArgument(
        "invalid http response for ReadObjectRange");
  }

  std::string const& content_range_value = loc->second;
  auto function = __func__;  // capture this function name, not the lambda's
  auto raise_error = [&content_range_value, &function]() {
    std::ostringstream os;
    os << static_cast<char const*>(function)
       << " invalid format for content-range header <" << content_range_value
       << ">";
    google::cloud::internal::ThrowInvalidArgument(os.str());
  };
  char unit_descriptor[] = "bytes";
  if (0 != content_range_value.find(unit_descriptor)) {
    raise_error();
  }
  char const* buffer = content_range_value.data();
  auto size = content_range_value.size();
  // skip the initial "bytes " string.
  buffer += sizeof(unit_descriptor);

  if (size < 2) {
    raise_error();
  }

  if (buffer[0] == '*' && buffer[1] == '/') {
    // The header is just the indication of size ('bytes */<size>'), parse that.
    buffer += 2;
    long long object_size;
    auto count = std::sscanf(buffer, "%lld", &object_size);
    if (count != 1) {
      raise_error();
    }
    return ReadObjectRangeResponse{std::move(response.payload), 0, 0,
                                   object_size};
  }

  long long first_byte;
  long long last_byte;
  long long object_size;
  auto count = std::sscanf(buffer, "%lld-%lld/%lld", &first_byte, &last_byte,
                           &object_size);
  if (count != 3) {
    raise_error();
  }

  return ReadObjectRangeResponse{std::move(response.payload), first_byte,
                                 last_byte, object_size};
}

std::ostream& operator<<(std::ostream& os, ReadObjectRangeResponse const& r) {
  return os << "ReadObjectRangeResponse={range=" << r.first_byte << "-"
            << r.last_byte << "/" << r.object_size << ", contents=\n"
            << BinaryDataAsDebugString(r.contents.data(), r.contents.size())
            << "}";
}

std::ostream& operator<<(std::ostream& os, DeleteObjectRequest const& r) {
  os << "DeleteObjectRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, UpdateObjectRequest const& r) {
  os << "UpdateObjectRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name() << ", metadata=" << r.metadata();
  r.DumpOptions(os, ", ");
  return os << "}";
}

ComposeObjectRequest::ComposeObjectRequest(
    std::string bucket_name, std::vector<ComposeSourceObject> source_objects,
    std::string destination_object_name)
    : GenericObjectRequest(std::move(bucket_name),
                           std::move(destination_object_name)),
      source_objects_(std::move(source_objects)) {}

std::string ComposeObjectRequest::JsonPayload() const {
  using ::google::cloud::storage::internal::nl::json;
  json compose_object_payload_json;
  compose_object_payload_json["kind"] = "storage#composeRequest";
  json destination_metadata_payload;
  if (HasOption<WithObjectMetadata>()) {
    destination_metadata_payload =
        ObjectMetadataJsonForCompose(GetOption<WithObjectMetadata>().value());
  }
  if (!destination_metadata_payload.is_null()) {
    compose_object_payload_json["destination"] = destination_metadata_payload;
  }
  json source_object_list;
  for (auto const& source_object : source_objects_) {
    json source_object_json;
    source_object_json["name"] = source_object.object_name;
    if (source_object.generation.has_value()) {
      source_object_json["generation"] = source_object.generation.value();
    }
    if (source_object.if_generation_match.has_value()) {
      source_object_json["ifGenerationMatch"] =
          source_object.if_generation_match.value();
    }
    source_object_list.emplace_back(std::move(source_object_json));
  }
  compose_object_payload_json["sourceObjects"] = source_object_list;

  return compose_object_payload_json.dump();
}

std::ostream& operator<<(std::ostream& os, ComposeObjectRequest const& r) {
  os << "ComposeObjectRequest={bucket_name=" << r.bucket_name()
     << ", destination_object_name=" << r.object_name();
  r.DumpOptions(os, ", ");
  return os << ", payload=" << r.JsonPayload() << "}";
}

PatchObjectRequest::PatchObjectRequest(std::string bucket_name,
                                       std::string object_name,
                                       ObjectMetadata const& original,
                                       ObjectMetadata const& updated)
    : GenericObjectRequest(std::move(bucket_name), std::move(object_name)) {
  // Compare each writeable field to build the patch.
  ObjectMetadataPatchBuilder builder;

  if (original.acl() != updated.acl()) {
    builder.SetAcl(updated.acl());
  }
  if (original.cache_control() != updated.cache_control()) {
    builder.SetCacheControl(updated.cache_control());
  }
  if (original.content_disposition() != updated.content_disposition()) {
    builder.SetContentDisposition(updated.content_disposition());
  }
  if (original.content_encoding() != updated.content_encoding()) {
    builder.SetContentEncoding(updated.content_encoding());
  }
  if (original.content_language() != updated.content_language()) {
    builder.SetContentLanguage(updated.content_language());
  }
  if (original.content_type() != updated.content_type()) {
    builder.SetContentType(updated.content_type());
  }
  if (original.event_based_hold() != updated.event_based_hold()) {
    builder.SetEventBasedHold(updated.event_based_hold());
  }

  if (original.metadata() != updated.metadata()) {
    if (updated.metadata().empty()) {
      builder.ResetMetadata();
    } else {
      std::map<std::string, std::string> difference;
      // Find the keys in the original map that are not in the new map. Using
      // `std::set_difference()` works because, unlike `std::unordered_map` the
      // `std::map` iterators return elements ordered by key:
      std::set_difference(original.metadata().begin(),
                          original.metadata().end(), updated.metadata().begin(),
                          updated.metadata().end(),
                          std::inserter(difference, difference.end()),
                          // We want to compare just keys and ignore values, the
                          // map class provides such a function, so use it:
                          original.metadata().value_comp());
      for (auto&& d : difference) {
        builder.ResetMetadata(d.first);
      }

      // Find the elements (comparing key and value) in the updated map that
      // are not in the original map:
      difference.clear();
      std::set_difference(updated.metadata().begin(), updated.metadata().end(),
                          original.metadata().begin(),
                          original.metadata().end(),
                          std::inserter(difference, difference.end()));
      for (auto&& d : difference) {
        builder.SetMetadata(d.first, d.second);
      }
    }
  }

  if (original.temporary_hold() != updated.temporary_hold()) {
    builder.SetTemporaryHold(updated.temporary_hold());
  }

  payload_ = builder.BuildPatch();
}

PatchObjectRequest::PatchObjectRequest(std::string bucket_name,
                                       std::string object_name,
                                       ObjectMetadataPatchBuilder const& patch)
    : GenericObjectRequest(std::move(bucket_name), std::move(object_name)),
      payload_(patch.BuildPatch()) {}

std::ostream& operator<<(std::ostream& os, PatchObjectRequest const& r) {
  os << "PatchObjectRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name();
  r.DumpOptions(os, ", ");
  return os << ", payload=" << r.payload() << "}";
}

std::ostream& operator<<(std::ostream& os, RewriteObjectRequest const& r) {
  os << "RewriteObjectRequest={destination_bucket=" << r.destination_bucket()
     << ", destination_object=" << r.destination_object()
     << ", source_bucket=" << r.source_bucket()
     << ", source_object=" << r.source_object()
     << ", rewrite_token=" << r.rewrite_token();
  r.DumpOptions(os, ", ");
  return os << "}";
}

StatusOr<RewriteObjectResponse> RewriteObjectResponse::FromHttpResponse(
    std::string const& payload) {
  nl::json object = nl::json::parse(payload, nullptr, false);
  if (!object.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }

  RewriteObjectResponse result;
  result.total_bytes_rewritten =
      ParseUnsignedLongField(object, "totalBytesRewritten");
  result.object_size = ParseUnsignedLongField(object, "objectSize");
  result.done = object.value("done", false);
  result.rewrite_token = object.value("rewriteToken", "");
  if (object.count("resource") != 0U) {
    auto parsed = internal::ObjectMetadataParser::FromJson(object["resource"]);
    if (!parsed.ok()) {
      return std::move(parsed).status();
    }
    result.resource = std::move(*parsed);
  }
  return result;
}

std::ostream& operator<<(std::ostream& os, RewriteObjectResponse const& r) {
  return os << "RewriteObjectResponse={total_bytes_rewritten="
            << r.total_bytes_rewritten << ", object_size=" << r.object_size
            << ", done=" << std::boolalpha << r.done
            << ", rewrite_token=" << r.rewrite_token
            << ", resource=" << r.resource << "}";
}

std::ostream& operator<<(std::ostream& os, ResumableUploadRequest const& r) {
  os << "ResumableUploadRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::string UploadChunkRequest::RangeHeader() const {
  std::ostringstream os;
  os << "Content-Range: bytes " << range_begin() << "-";
  if (payload().empty()) {
    // This typically happens when the sender realizes too late that the
    // previous chunk was really the last chunk (e.g. the file is exactly a
    // multiple of the quantum, reading the last chunk does not detect the EOF),
    // the formatting of the range is special in this case.
    os << range_begin();
  } else {
    os << range_begin() + payload().size() - 1;
  }
  if (source_size() == 0) {
    os << "/*";
  } else {
    os << "/" << source_size();
  }
  return std::move(os).str();
}

std::ostream& operator<<(std::ostream& os, UploadChunkRequest const& r) {
  os << "UploadChunkRequest={upload_session_url=" << r.upload_session_url()
     << ", range=<" << r.RangeHeader() << ">";
  r.DumpOptions(os, ", ");
  return os << ", payload="
            << BinaryDataAsDebugString(r.payload().data(), r.payload().size(),
                                       128)
            << "}";
}

std::ostream& operator<<(std::ostream& os,
                         QueryResumableUploadRequest const& r) {
  os << "QueryResumableUploadRequest={upload_session_url="
     << r.upload_session_url();
  r.DumpOptions(os, ", ");
  return os << "}";
}

StatusOr<ResumableUploadResponse> ResumableUploadResponse::FromHttpResponse(
    HttpResponse&& response) {
  ResumableUploadResponse result;
  result.last_committed_byte = 0;
  result.payload = std::move(response.payload);
  if (response.headers.find("location") != response.headers.end()) {
    result.upload_session_url = response.headers.find("location")->second;
  }
  if (response.headers.find("range") == response.headers.end()) {
    return result;
  }
  // We expect a `Range:` header in the format described here:
  //    https://cloud.google.com/storage/docs/json_api/v1/how-tos/resumable-upload
  // that is the value should match `bytes=0-[0-9]+`:
  std::string const& range = response.headers.find("range")->second;

  if (range.rfind("bytes=0-", 0) != 0) {
    return result;
  }
  char const* buffer = range.data() + 8;
  char* endptr;
  auto last = std::strtoll(buffer, &endptr, 10);
  if (*endptr == '\0' && 0 <= last) {
    result.last_committed_byte = static_cast<std::uint64_t>(last);
  }

  return result;
}

std::ostream& operator<<(std::ostream& os, ResumableUploadResponse const& r) {
  return os << "ResumableUploadResponse={upload_session_url="
            << r.upload_session_url
            << ", last_committed_byte=" << r.last_committed_byte << ", payload="
            << BinaryDataAsDebugString(r.payload.data(), r.payload.size(), 128)
            << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

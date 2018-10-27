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
#include "google/cloud/storage/object_metadata.h"
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
std::ostream& operator<<(std::ostream& os, ListObjectsRequest const& r) {
  os << "ListObjectsRequest={bucket_name=" << r.bucket_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

ListObjectsResponse ListObjectsResponse::FromHttpResponse(
    HttpResponse&& response) {
  auto json = storage::internal::nl::json::parse(response.payload);

  ListObjectsResponse result;
  result.next_page_token = json.value("nextPageToken", "");

  for (auto const& kv : json["items"].items()) {
    result.items.emplace_back(ObjectMetadata::ParseFromJson(kv.value()));
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
  return os << ", payload=" << r.json_payload() << "}";
}

std::ostream& operator<<(std::ostream& os, ReadObjectRangeRequest const& r) {
  os << "ReadObjectRangeRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name() << ", begin=" << r.begin()
     << ", end=" << r.end();
  r.DumpOptions(os, ", ");
  return os << "}";
}

ReadObjectRangeResponse ReadObjectRangeResponse::FromHttpResponse(
    HttpResponse&& response) {
  auto loc = response.headers.find(std::string("content-range"));
  if (response.headers.end() == loc) {
    google::cloud::internal::RaiseInvalidArgument(
        "invalid http response for ReadObjectRange");
  }

  std::string const& content_range_value = loc->second;
  auto function = __func__;  // capture this function name, not the lambda's
  auto raise_error = [&content_range_value, &function]() {
    std::ostringstream os;
    os << static_cast<char const*>(function)
       << " invalid format for content-range header <" << content_range_value
       << ">";
    google::cloud::internal::RaiseInvalidArgument(os.str());
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

  if (buffer[0] == '*' and buffer[1] == '/') {
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
    std::string bucket_name,
    std::vector<ComposeSourceObject> const& source_objects,
    std::string destination_object_name,
    ObjectMetadata destination_object_metadata)
    : GenericObjectRequest(std::move(bucket_name),
                           std::move(destination_object_name)),
      destination_metadata_(std::move(destination_object_metadata)) {
  using internal::nl::json;
  json compose_object_payload_json;
  compose_object_payload_json["kind"] = "storage#composeRequest";
  auto destination_metadata_payload =
      destination_metadata_.JsonPayloadForCompose();
  if (!destination_metadata_payload.is_null()) {
    compose_object_payload_json["destination"] = destination_metadata_payload;
  }
  json source_object_list;
  for (auto const& source_object : source_objects) {
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
  json_payload_ = compose_object_payload_json.dump();
}

std::ostream& operator<<(std::ostream& os, ComposeObjectRequest const& r) {
  os << "ComposeObjectRequest={bucket_name=" << r.bucket_name()
     << ", destination_object_name=" << r.object_name()
     << ", destination_metadata="
     << r.destination_metadata().JsonPayloadForCompose();
  r.DumpOptions(os, ", ");
  return os << ", payload=" << r.json_payload() << "}";
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
  return os << ", payload=" << r.json_payload() << "}";
}

RewriteObjectResponse RewriteObjectResponse::FromHttpResponse(
    HttpResponse const& response) {
  nl::json object = nl::json::parse(response.payload);
  RewriteObjectResponse result;
  result.total_bytes_rewritten =
      ParseUnsignedLongField(object, "totalBytesRewritten");
  result.object_size = ParseUnsignedLongField(object, "objectSize");
  result.done = object.value("done", false);
  result.rewrite_token = object.value("rewriteToken", "");
  if (object.count("resource") != 0U) {
    result.resource = ObjectMetadata::ParseFromJson(object["resource"]);
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

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

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
#include "google/cloud/storage/internal/object_acl_requests.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/object_metadata.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include <cinttypes>
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
std::string UpdateObjectRequest::json_payload() const {
  return ObjectMetadataJsonForUpdate(metadata_).dump();
}

std::ostream& operator<<(std::ostream& os, ListObjectsRequest const& r) {
  os << "ListObjectsRequest={bucket_name=" << r.bucket_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

StatusOr<ListObjectsResponse> ListObjectsResponse::FromHttpResponse(
    std::string const& payload) {
  auto json = nlohmann::json::parse(payload, nullptr, false);
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

  for (auto const& prefix_iterator : json["prefixes"].items()) {
    auto const& prefix = prefix_iterator.value();
    if (!prefix.is_string()) {
      return Status(StatusCode::kInternal,
                    "List Objects Response's 'prefix' is not a string.");
    }
    result.prefixes.emplace_back(prefix.get<std::string>());
  }

  return result;
}

std::ostream& operator<<(std::ostream& os, ListObjectsResponse const& r) {
  os << "ListObjectsResponse={next_page_token=" << r.next_page_token
     << ", items={";
  std::copy(r.items.begin(), r.items.end(),
            std::ostream_iterator<ObjectMetadata>(os, "\n  "));
  os << "}, prefixes={";
  std::copy(r.prefixes.begin(), r.prefixes.end(),
            std::ostream_iterator<std::string>(os, "\n "));
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
  std::size_t constexpr kMaxDumpSize = 1024;
  if (r.contents().size() > kMaxDumpSize) {
    os << ", contents[0..1024]=\n"
       << BinaryDataAsDebugString(r.contents().data(), kMaxDumpSize);
  } else {
    os << ", contents=\n"
       << BinaryDataAsDebugString(r.contents().data(), r.contents().size());
  }
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

bool ReadObjectRangeRequest::RequiresNoCache() const {
  if (HasOption<ReadRange>()) {
    return true;
  }
  if (HasOption<ReadFromOffset>() && GetOption<ReadFromOffset>().value() != 0) {
    return true;
  }
  return HasOption<ReadLast>();
}

bool ReadObjectRangeRequest::RequiresRangeHeader() const {
  return RequiresNoCache();
}

std::string ReadObjectRangeRequest::RangeHeader() const {
  if (HasOption<ReadRange>() && HasOption<ReadFromOffset>()) {
    auto range = GetOption<ReadRange>().value();
    auto offset = GetOption<ReadFromOffset>().value();
    auto begin = (std::max)(range.begin, offset);
    return "Range: bytes=" + std::to_string(begin) + "-" +
           std::to_string(range.end - 1);
  }
  if (HasOption<ReadRange>()) {
    auto range = GetOption<ReadRange>().value();
    return "Range: bytes=" + std::to_string(range.begin) + "-" +
           std::to_string(range.end - 1);
  }
  if (HasOption<ReadFromOffset>()) {
    auto offset = GetOption<ReadFromOffset>().value();
    if (offset != 0) {
      return "Range: bytes=" + std::to_string(offset) + "-";
    }
  }
  if (HasOption<ReadLast>()) {
    auto last = GetOption<ReadLast>().value();
    return "Range: bytes=-" + std::to_string(last);
  }
  return "";
}

std::int64_t ReadObjectRangeRequest::StartingByte() const {
  std::int64_t result = 0;
  if (HasOption<ReadRange>()) {
    result = (std::max)(result, GetOption<ReadRange>().value().begin);
  }
  if (HasOption<ReadFromOffset>()) {
    result = (std::max)(result, GetOption<ReadFromOffset>().value());
  }
  if (HasOption<ReadLast>()) {
    // The value of `StartingByte()` is unknown if `ReadLast` is set
    result = -1;
  }
  return result;
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
  // capture this function name, not the lambda's
  auto const* function = __func__;
  auto raise_error = [&content_range_value, &function]() {
    std::ostringstream os;
    os << static_cast<char const*>(function)
       << " invalid format for content-range header <" << content_range_value
       << ">";
    google::cloud::internal::ThrowInvalidArgument(os.str());
  };
  char const unit_descriptor[] = "bytes";
  if (content_range_value.rfind(unit_descriptor, 0) != 0) {
    raise_error();
  }
  char const* buffer = content_range_value.data();
  auto size = content_range_value.size();
  if (size < sizeof(unit_descriptor) + 2) raise_error();

  // skip the initial "bytes " string.
  buffer += sizeof(unit_descriptor);
  size -= sizeof(unit_descriptor);

  if (buffer[0] == '*' && buffer[1] == '/') {
    // The header is just the indication of size ('bytes */<size>'), parse that.
    std::int64_t object_size;
    if (!absl::SimpleAtoi(absl::string_view(buffer + 2, size - 2),
                          &object_size)) {
      raise_error();
    }
    return ReadObjectRangeResponse{std::move(response.payload), 0, 0,
                                   object_size};
  }

  std::vector<absl::string_view> components =
      absl::StrSplit(absl::string_view(buffer, size), absl::ByAnyChar("-/"));
  if (components.size() != 3) raise_error();

  std::int64_t first_byte;
  std::int64_t last_byte;
  std::int64_t object_size;
  if (!absl::SimpleAtoi(components[0], &first_byte)) raise_error();
  if (!absl::SimpleAtoi(components[1], &last_byte)) raise_error();
  if (!absl::SimpleAtoi(components[2], &object_size)) raise_error();

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
  nlohmann::json compose_object_payload_json;
  compose_object_payload_json["kind"] = "storage#composeRequest";
  nlohmann::json destination_metadata_payload;
  if (HasOption<WithObjectMetadata>()) {
    destination_metadata_payload =
        ObjectMetadataJsonForCompose(GetOption<WithObjectMetadata>().value());
  }
  if (!destination_metadata_payload.is_null()) {
    compose_object_payload_json["destination"] = destination_metadata_payload;
  }
  nlohmann::json source_object_list;
  for (auto const& source_object : source_objects_) {
    nlohmann::json source_object_json;
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
  auto object = nlohmann::json::parse(payload, nullptr, false);
  if (!object.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }

  RewriteObjectResponse result;
  result.total_bytes_rewritten =
      ParseUnsignedLongField(object, "totalBytesRewritten");
  result.object_size = ParseUnsignedLongField(object, "objectSize");
  result.done = object.value("done", false);
  result.rewrite_token = object.value("rewriteToken", "");
  if (object.count("resource") != 0) {
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

std::ostream& operator<<(std::ostream& os,
                         DeleteResumableUploadRequest const& r) {
  os << "DeleteResumableUploadRequest={upload_session_url="
     << r.upload_session_url();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::string UploadChunkRequest::RangeHeader() const {
  std::ostringstream os;
  os << "Content-Range: bytes ";
  auto const size = payload_size();
  if (size == 0) {
    // This typically happens when the sender realizes too late that the
    // previous chunk was really the last chunk (e.g. the file is exactly a
    // multiple of the quantum, reading the last chunk from a file, or sending
    // it as part of a stream that does not detect the EOF), the formatting of
    // the range is special in this case.
    os << "*";
  } else {
    os << range_begin() << "-" << range_begin() + size - 1;
  }
  if (!last_chunk_) {
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
  os << ", payload={";
  auto constexpr kMaxOutputBytes = 128;
  char const* sep = "";
  for (auto const& b : r.payload()) {
    os << sep << "{"
       << BinaryDataAsDebugString(b.data(), b.size(), kMaxOutputBytes) << "}";
    sep = ", ";
  }
  return os << "}}";
}

std::ostream& operator<<(std::ostream& os,
                         QueryResumableUploadRequest const& r) {
  os << "QueryResumableUploadRequest={upload_session_url="
     << r.upload_session_url();
  r.DumpOptions(os, ", ");
  return os << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

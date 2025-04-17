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

#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/internal/binary_data_as_debug_string.h"
#include "google/cloud/storage/internal/metadata_parser.h"
#include "google/cloud/storage/internal/object_acl_requests.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/object_metadata.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include <algorithm>
#include <cinttypes>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

ObjectMetadataPatchBuilder DiffObjectMetadata(ObjectMetadata const& original,
                                              ObjectMetadata const& updated) {
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

  return builder;
}

}  // namespace

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
  if (!json.is_object()) return ExpectedJsonObject(payload, GCP_ERROR_INFO());

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
      return google::cloud::internal::InternalError(
          "List Objects Response's 'prefix' is not a string.",
          GCP_ERROR_INFO());
    }
    result.prefixes.emplace_back(prefix.get<std::string>());
  }

  return result;
}

StatusOr<ListObjectsResponse> ListObjectsResponse::FromHttpResponse(
    HttpResponse const& response) {
  return FromHttpResponse(response.payload);
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

InsertObjectMediaRequest::InsertObjectMediaRequest() { reset_hash_function(); }

InsertObjectMediaRequest::InsertObjectMediaRequest(std::string bucket_name,
                                                   std::string object_name,
                                                   absl::string_view payload)
    : InsertObjectRequestImpl<InsertObjectMediaRequest>(std::move(bucket_name),
                                                        std::move(object_name)),
      payload_(payload) {
  reset_hash_function();
}

void InsertObjectMediaRequest::reset_hash_function() {
  hash_function_ = CreateHashFunction(
      GetOption<Crc32cChecksumValue>(), GetOption<DisableCrc32cChecksum>(),
      GetOption<MD5HashValue>(), GetOption<DisableMD5Hash>());
}

void InsertObjectMediaRequest::set_payload(absl::string_view payload) {
  payload_ = payload;
  dirty_ = true;
}

std::string const& InsertObjectMediaRequest::contents() const {
  if (!dirty_) return contents_;
  contents_ = std::string{payload_};
  dirty_ = false;
  return contents_;
}

void InsertObjectMediaRequest::set_contents(std::string v) {
  contents_ = std::move(v);
  payload_ = contents_;
  dirty_ = false;
}

HashValues FinishHashes(InsertObjectMediaRequest const& request) {
  return request.hash_function().Finish();
}

std::ostream& operator<<(std::ostream& os, InsertObjectMediaRequest const& r) {
  os << "InsertObjectMediaRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name();
  r.DumpOptions(os, ", ");
  std::size_t constexpr kMaxDumpSize = 128;
  auto const payload = r.payload();
  os << ", contents="
     << BinaryDataAsDebugString(payload.data(), payload.size(), kMaxDumpSize);
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

std::string ReadObjectRangeRequest::RangeHeaderValue() const {
  if (HasOption<ReadRange>() && HasOption<ReadFromOffset>()) {
    auto range = GetOption<ReadRange>().value();
    auto offset = GetOption<ReadFromOffset>().value();
    auto begin = (std::max)(range.begin, offset);
    return "bytes=" + std::to_string(begin) + "-" +
           std::to_string(range.end - 1);
  }
  if (HasOption<ReadRange>()) {
    auto range = GetOption<ReadRange>().value();
    return "bytes=" + std::to_string(range.begin) + "-" +
           std::to_string(range.end - 1);
  }
  if (HasOption<ReadFromOffset>()) {
    auto offset = GetOption<ReadFromOffset>().value();
    if (offset != 0) {
      return "bytes=" + std::to_string(offset) + "-";
    }
  }
  if (HasOption<ReadLast>()) {
    auto last = GetOption<ReadLast>().value();
    return "bytes=-" + std::to_string(last);
  }
  return "";
}

std::string ReadObjectRangeRequest::RangeHeader() const {
  auto value = RangeHeaderValue();
  if (value.empty()) return "";
  return "Range: " + value;
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
    : PatchObjectRequest(std::move(bucket_name), std::move(object_name),
                         DiffObjectMetadata(original, updated)) {}

PatchObjectRequest::PatchObjectRequest(std::string bucket_name,
                                       std::string object_name,
                                       ObjectMetadataPatchBuilder patch)
    : GenericObjectRequest(std::move(bucket_name), std::move(object_name)),
      patch_(std::move(patch)) {}

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

std::ostream& operator<<(std::ostream& os, MoveObjectRequest const& r) {
  os << "MoveObjectRequest={bucket_name=" << r.bucket_name()
     << ", source_object_name=" << r.source_object_name()
     << ", destination_object_name=" << r.destination_object_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, RestoreObjectRequest const& r) {
  os << "RestoreObjectRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name()
     << ", generation=" << r.generation();
  r.DumpOptions(os, ", ");
  return os << "}";
}

StatusOr<RewriteObjectResponse> RewriteObjectResponse::FromHttpResponse(
    std::string const& payload) {
  auto object = nlohmann::json::parse(payload, nullptr, false);
  if (!object.is_object()) return ExpectedJsonObject(payload, GCP_ERROR_INFO());

  RewriteObjectResponse result;
  auto v = ParseUnsignedLongField(object, "totalBytesRewritten");
  if (!v) return std::move(v).status();
  result.total_bytes_rewritten = *v;
  v = ParseUnsignedLongField(object, "objectSize");
  if (!v) return std::move(v).status();
  result.object_size = *v;
  result.done = object.value("done", false);
  result.rewrite_token = object.value("rewriteToken", "");
  if (object.count("resource") != 0) {
    auto parsed = internal::ObjectMetadataParser::FromJson(object["resource"]);
    if (!parsed.ok()) return std::move(parsed).status();
    result.resource = std::move(*parsed);
  }
  return result;
}

StatusOr<RewriteObjectResponse> RewriteObjectResponse::FromHttpResponse(
    HttpResponse const& response) {
  return FromHttpResponse(response.payload);
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

StatusOr<CreateResumableUploadResponse>
CreateResumableUploadResponse::FromHttpResponse(HttpResponse response) {
  if (response.headers.find("location") == response.headers.end()) {
    return google::cloud::internal::InternalError("Missing location header",
                                                  GCP_ERROR_INFO());
  }
  return CreateResumableUploadResponse{
      response.headers.find("location")->second};
}

bool operator==(CreateResumableUploadResponse const& lhs,
                CreateResumableUploadResponse const& rhs) {
  return lhs.upload_id == rhs.upload_id;
}

std::ostream& operator<<(std::ostream& os,
                         CreateResumableUploadResponse const& r) {
  return os << "CreateResumableUploadResponse={upload_id=" << r.upload_id
            << "}";
}

UploadChunkRequest::UploadChunkRequest(
    std::string upload_session_url, std::uint64_t offset,
    ConstBufferSequence payload, std::shared_ptr<HashFunction> hash_function)
    : upload_session_url_(std::move(upload_session_url)),
      offset_(offset),
      payload_(std::move(payload)),
      hash_function_(std::move(hash_function)) {}

UploadChunkRequest::UploadChunkRequest(
    std::string upload_session_url, std::uint64_t offset,
    ConstBufferSequence payload, std::shared_ptr<HashFunction> hash_function,
    HashValues known_hashes)
    : upload_session_url_(std::move(upload_session_url)),
      offset_(offset),
      upload_size_(offset + TotalBytes(payload)),
      payload_(std::move(payload)),
      hash_function_(std::move(hash_function)),
      known_object_hashes_(std::move(known_hashes)) {}

std::string UploadChunkRequest::RangeHeaderValue() const {
  std::ostringstream os;
  os << "bytes ";
  auto const size = payload_size();
  if (size == 0) {
    // This typically happens when the sender realizes too late that the
    // previous chunk was really the last chunk (e.g. the file is exactly a
    // multiple of the quantum, reading the last chunk from a file, or sending
    // it as part of a stream that does not detect the EOF), the formatting of
    // the range is special in this case.
    os << "*";
  } else {
    os << offset() << "-" << offset() + size - 1;
  }
  if (!upload_size().has_value()) {
    os << "/*";
  } else {
    os << "/" << *upload_size();
  }
  return std::move(os).str();
}

std::string UploadChunkRequest::RangeHeader() const {
  return "Content-Range: " + RangeHeaderValue();
}

UploadChunkRequest UploadChunkRequest::RemainingChunk(
    std::uint64_t new_offset) const {
  UploadChunkRequest result = *this;
  if (new_offset < offset_ || new_offset >= offset_ + payload_size()) {
    result.offset_ = new_offset;
    result.payload_.clear();
    return result;
  }
  // No chunk can be larger than what fits in memory, and because `new_offset`
  // is in `[offset_, offset_ + payload_size())`, this static_cast<> is safe:
  PopFrontBytes(result.payload_,
                static_cast<std::size_t>(new_offset - result.offset_));
  result.offset_ = new_offset;
  return result;
}

HashValues FinishHashes(UploadChunkRequest const& request) {
  // Prefer the hashes provided via *Value options in the request. If those
  // are not set, use the computed hashes from the data.
  return Merge(request.known_object_hashes(), request.hash_function().Finish());
}

std::ostream& operator<<(std::ostream& os, UploadChunkRequest const& r) {
  os << "UploadChunkRequest={upload_session_url=" << r.upload_session_url()
     << ", range=<" << r.RangeHeader() << ">"
     << ", known_object_hashes={" << Format(r.known_object_hashes()) << "}";
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

StatusOr<std::uint64_t> ParseRangeHeader(std::string const& range) {
  // We expect a `Range:` header in the format described here:
  //    https://cloud.google.com/storage/docs/json_api/v1/how-tos/resumable-upload
  // that is the value should match `bytes=0-[0-9]+`:
  char const prefix[] = "bytes=0-";
  auto constexpr kPrefixLen = sizeof(prefix) - 1;
  if (!absl::StartsWith(range, prefix)) {
    return google::cloud::internal::InternalError(
        "cannot parse Range header in resumable upload response, value=" +
            range,
        GCP_ERROR_INFO());
  }
  char const* buffer = range.data() + kPrefixLen;
  char* endptr;
  auto constexpr kBytesBase = 10;
  auto last = std::strtoll(buffer, &endptr, kBytesBase);
  if (buffer != endptr && *endptr == '\0' && 0 <= last) {
    return last;
  }
  return google::cloud::internal::InternalError(
      "cannot parse Range header in resumable uload response, value=" + range,
      GCP_ERROR_INFO());
}

StatusOr<QueryResumableUploadResponse>
QueryResumableUploadResponse::FromHttpResponse(HttpResponse response) {
  QueryResumableUploadResponse result;
  result.request_metadata = std::move(response.headers);
  auto done = response.status_code == HttpStatusCode::kOk ||
              response.status_code == HttpStatusCode::kCreated;

  // For the JSON API, the payload contains the object resource when the upload
  // is finished. In that case, we try to parse it.
  if (done && !response.payload.empty()) {
    auto contents = ObjectMetadataParser::FromString(response.payload);
    if (!contents) return std::move(contents).status();
    result.payload = *std::move(contents);
  }
  auto r = result.request_metadata.find("range");
  if (r == result.request_metadata.end()) return result;

  auto last_committed_byte = ParseRangeHeader(r->second);
  if (!last_committed_byte) return std::move(last_committed_byte).status();
  result.committed_size = *last_committed_byte + 1;

  return result;
}

bool operator==(QueryResumableUploadResponse const& lhs,
                QueryResumableUploadResponse const& rhs) {
  return lhs.committed_size == rhs.committed_size && lhs.payload == rhs.payload;
}

std::ostream& operator<<(std::ostream& os,
                         QueryResumableUploadResponse const& r) {
  os << "UploadChunkResponse={committed_size=";
  if (r.committed_size.has_value()) {
    os << *r.committed_size;
  } else {
    os << "{}";
  }
  os << ", payload=";
  if (r.payload.has_value()) {
    os << *r.payload;
  } else {
    os << "{}";
  }
  return os << "}";
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

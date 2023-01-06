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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_REQUESTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_REQUESTS_H

#include "google/cloud/storage/auto_finalize.h"
#include "google/cloud/storage/download_options.h"
#include "google/cloud/storage/hashing_options.h"
#include "google/cloud/storage/internal/const_buffer.h"
#include "google/cloud/storage/internal/generic_object_request.h"
#include "google/cloud/storage/internal/hash_values.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/upload_options.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/storage/well_known_parameters.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include <map>
#include <numeric>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
/**
 * Represents a request to the `Objects: list` API.
 */
class ListObjectsRequest
    : public GenericRequest<ListObjectsRequest, MaxResults, Prefix, Delimiter,
                            IncludeTrailingDelimiter, StartOffset, EndOffset,
                            Projection, UserProject, Versions> {
 public:
  ListObjectsRequest() = default;
  explicit ListObjectsRequest(std::string bucket_name)
      : bucket_name_(std::move(bucket_name)) {}

  std::string const& bucket_name() const { return bucket_name_; }
  std::string const& page_token() const { return page_token_; }
  ListObjectsRequest& set_page_token(std::string page_token) {
    page_token_ = std::move(page_token);
    return *this;
  }

 private:
  std::string bucket_name_;
  std::string page_token_;
};

std::ostream& operator<<(std::ostream& os, ListObjectsRequest const& r);

struct ListObjectsResponse {
  static StatusOr<ListObjectsResponse> FromHttpResponse(
      std::string const& payload);
  static StatusOr<ListObjectsResponse> FromHttpResponse(
      HttpResponse const& response);

  std::string next_page_token;
  std::vector<ObjectMetadata> items;
  std::vector<std::string> prefixes;
};

std::ostream& operator<<(std::ostream& os, ListObjectsResponse const& r);

/**
 * Represents a request to the `Objects: get` API.
 */
class GetObjectMetadataRequest
    : public GenericObjectRequest<
          GetObjectMetadataRequest, Generation, IfGenerationMatch,
          IfGenerationNotMatch, IfMetagenerationMatch, IfMetagenerationNotMatch,
          Projection, UserProject> {
 public:
  using GenericObjectRequest::GenericObjectRequest;
};

std::ostream& operator<<(std::ostream& os, GetObjectMetadataRequest const& r);

/**
 * Represents a request to the `Objects: insert` API with a string for the
 * media.
 *
 * This request type is used to upload objects with media that completely
 * fits in memory. Such requests are simpler than uploading objects streaming
 * objects.
 */
class InsertObjectMediaRequest
    : public GenericObjectRequest<
          InsertObjectMediaRequest, ContentEncoding, ContentType,
          Crc32cChecksumValue, DisableCrc32cChecksum, DisableMD5Hash,
          EncryptionKey, IfGenerationMatch, IfGenerationNotMatch,
          IfMetagenerationMatch, IfMetagenerationNotMatch, KmsKeyName,
          MD5HashValue, PredefinedAcl, Projection, UserProject,
          UploadFromOffset, UploadLimit, WithObjectMetadata> {
 public:
  InsertObjectMediaRequest() = default;

  explicit InsertObjectMediaRequest(std::string bucket_name,
                                    std::string object_name,
                                    std::string contents)
      : GenericObjectRequest(std::move(bucket_name), std::move(object_name)),
        contents_(std::move(contents)) {}

  std::string const& contents() const { return contents_; }
  InsertObjectMediaRequest& set_contents(std::string&& v) {
    contents_ = std::move(v);
    return *this;
  }

 private:
  std::string contents_;
};

std::ostream& operator<<(std::ostream& os, InsertObjectMediaRequest const& r);

/**
 * Represents a request to the `Objects: copy` API.
 */
class CopyObjectRequest
    : public GenericRequest<
          CopyObjectRequest, DestinationKmsKeyName, DestinationPredefinedAcl,
          EncryptionKey, IfGenerationMatch, IfGenerationNotMatch,
          IfMetagenerationMatch, IfMetagenerationNotMatch,
          IfSourceGenerationMatch, IfSourceGenerationNotMatch,
          IfSourceMetagenerationMatch, IfSourceMetagenerationNotMatch,
          Projection, SourceGeneration, SourceEncryptionKey, UserProject,
          WithObjectMetadata> {
 public:
  CopyObjectRequest() = default;
  CopyObjectRequest(std::string source_bucket, std::string source_object,
                    std::string destination_bucket,
                    std::string destination_object)
      : source_bucket_(std::move(source_bucket)),
        source_object_(std::move(source_object)),
        destination_bucket_(std::move(destination_bucket)),
        destination_object_(std::move(destination_object)) {}

  std::string const& source_bucket() const { return source_bucket_; }
  std::string const& source_object() const { return source_object_; }
  std::string const& destination_bucket() const { return destination_bucket_; }
  std::string const& destination_object() const { return destination_object_; }

 private:
  std::string source_bucket_;
  std::string source_object_;
  std::string destination_bucket_;
  std::string destination_object_;
};

std::ostream& operator<<(std::ostream& os, CopyObjectRequest const& r);

/**
 * Represents a request to the `Objects: get` API with `alt=media`.
 */
class ReadObjectRangeRequest
    : public GenericObjectRequest<
          ReadObjectRangeRequest, DisableCrc32cChecksum, DisableMD5Hash,
          EncryptionKey, Generation, IfGenerationMatch, IfGenerationNotMatch,
          IfMetagenerationMatch, IfMetagenerationNotMatch, ReadFromOffset,
          ReadRange, ReadLast, UserProject, AcceptEncoding> {
 public:
  using GenericObjectRequest::GenericObjectRequest;

  bool RequiresNoCache() const;
  bool RequiresRangeHeader() const;
  std::string RangeHeader() const;
  std::string RangeHeaderValue() const;
  std::int64_t StartingByte() const;
};

std::ostream& operator<<(std::ostream& os, ReadObjectRangeRequest const& r);

/**
 * Represents a request to the `Objects: delete` API.
 */
class DeleteObjectRequest
    : public GenericObjectRequest<DeleteObjectRequest, Generation,
                                  IfGenerationMatch, IfGenerationNotMatch,
                                  IfMetagenerationMatch,
                                  IfMetagenerationNotMatch, UserProject> {
 public:
  using GenericObjectRequest::GenericObjectRequest;
};

std::ostream& operator<<(std::ostream& os, DeleteObjectRequest const& r);

/**
 * Represents a request to the `Objects: update` API.
 */
class UpdateObjectRequest
    : public GenericObjectRequest<
          UpdateObjectRequest, Generation, EncryptionKey, IfGenerationMatch,
          IfGenerationNotMatch, IfMetagenerationMatch, IfMetagenerationNotMatch,
          PredefinedAcl, Projection, UserProject> {
 public:
  UpdateObjectRequest() = default;
  explicit UpdateObjectRequest(std::string bucket_name, std::string object_name,
                               ObjectMetadata metadata)
      : GenericObjectRequest(std::move(bucket_name), std::move(object_name)),
        metadata_(std::move(metadata)) {}

  /// Returns the request as the JSON API payload.
  std::string json_payload() const;

  ObjectMetadata const& metadata() const { return metadata_; }

 private:
  ObjectMetadata metadata_;
};

std::ostream& operator<<(std::ostream& os, UpdateObjectRequest const& r);

/**
 * Represents a request to the `Objects: compose` API.
 */
class ComposeObjectRequest
    : public GenericObjectRequest<ComposeObjectRequest, EncryptionKey,
                                  DestinationPredefinedAcl, KmsKeyName,
                                  IfGenerationMatch, IfMetagenerationMatch,
                                  UserProject, WithObjectMetadata> {
 public:
  ComposeObjectRequest() = default;
  explicit ComposeObjectRequest(std::string bucket_name,
                                std::vector<ComposeSourceObject> source_objects,
                                std::string destination_object_name);

  std::vector<ComposeSourceObject> source_objects() const {
    return source_objects_;
  }

  /// Returns the request as the JSON API payload.
  std::string JsonPayload() const;

 private:
  std::vector<ComposeSourceObject> source_objects_;
};

std::ostream& operator<<(std::ostream& os, ComposeObjectRequest const& r);

/**
 * Represents a request to the `Objects: patch` API.
 */
class PatchObjectRequest
    : public GenericObjectRequest<
          PatchObjectRequest, Generation, IfGenerationMatch,
          IfGenerationNotMatch, IfMetagenerationMatch, IfMetagenerationNotMatch,
          PredefinedAcl, EncryptionKey, Projection, UserProject,
          // PredefinedDefaultObjectAcl has no effect in an `Objects: patch`
          // request.  We are keeping it here for backwards compatibility. It
          // was introduced in error (should have been PredefinedAcl), and it
          // was never documented. The cost of keeping it is small, and there
          // is very little motivation to remove it.
          PredefinedDefaultObjectAcl> {
 public:
  PatchObjectRequest() = default;
  explicit PatchObjectRequest(std::string bucket_name, std::string object_name,
                              ObjectMetadata const& original,
                              ObjectMetadata const& updated);
  explicit PatchObjectRequest(std::string bucket_name, std::string object_name,
                              ObjectMetadataPatchBuilder patch);

  ObjectMetadataPatchBuilder const& patch() const { return patch_; }
  std::string payload() const { return patch_.BuildPatch(); }

 private:
  ObjectMetadataPatchBuilder patch_;
};

std::ostream& operator<<(std::ostream& os, PatchObjectRequest const& r);

/**
 * Represents a request to the `Objects: rewrite` API.
 */
class RewriteObjectRequest
    : public GenericRequest<
          RewriteObjectRequest, DestinationKmsKeyName, DestinationPredefinedAcl,
          EncryptionKey, IfGenerationMatch, IfGenerationNotMatch,
          IfMetagenerationMatch, IfMetagenerationNotMatch,
          IfSourceGenerationMatch, IfSourceGenerationNotMatch,
          IfSourceMetagenerationMatch, IfSourceMetagenerationNotMatch,
          MaxBytesRewrittenPerCall, Projection, SourceEncryptionKey,
          SourceGeneration, UserProject, WithObjectMetadata> {
 public:
  RewriteObjectRequest() = default;
  RewriteObjectRequest(std::string source_bucket, std::string source_object,
                       std::string destination_bucket,
                       std::string destination_object,
                       std::string rewrite_token)
      : source_bucket_(std::move(source_bucket)),
        source_object_(std::move(source_object)),
        destination_bucket_(std::move(destination_bucket)),
        destination_object_(std::move(destination_object)),
        rewrite_token_(std::move(rewrite_token)) {}

  std::string const& source_bucket() const { return source_bucket_; }
  std::string const& source_object() const { return source_object_; }
  std::string const& destination_bucket() const { return destination_bucket_; }
  std::string const& destination_object() const { return destination_object_; }
  std::string const& rewrite_token() const { return rewrite_token_; }
  void set_rewrite_token(std::string v) { rewrite_token_ = std::move(v); }

 private:
  std::string source_bucket_;
  std::string source_object_;
  std::string destination_bucket_;
  std::string destination_object_;
  std::string rewrite_token_;
};

std::ostream& operator<<(std::ostream& os, RewriteObjectRequest const& r);

/// Holds an `Objects: rewrite` response.
struct RewriteObjectResponse {
  static StatusOr<RewriteObjectResponse> FromHttpResponse(
      std::string const& payload);
  static StatusOr<RewriteObjectResponse> FromHttpResponse(
      HttpResponse const& response);

  std::uint64_t total_bytes_rewritten;
  std::uint64_t object_size;
  bool done;
  std::string rewrite_token;
  ObjectMetadata resource;
};

std::ostream& operator<<(std::ostream& os, RewriteObjectResponse const& r);

/**
 * Represents a request to start a resumable upload in `Objects: insert`.
 *
 * This request type is used to start resumable uploads. A resumable upload is
 * started with a `Objects: insert` request with the `uploadType=resumable`
 * query parameter. The payload for the initial request includes the (optional)
 * object metadata. The response includes a URL to send requests that upload
 * the media.
 */
class ResumableUploadRequest
    : public GenericObjectRequest<
          ResumableUploadRequest, ContentEncoding, ContentType,
          Crc32cChecksumValue, DisableCrc32cChecksum, DisableMD5Hash,
          EncryptionKey, IfGenerationMatch, IfGenerationNotMatch,
          IfMetagenerationMatch, IfMetagenerationNotMatch, KmsKeyName,
          MD5HashValue, PredefinedAcl, Projection, UseResumableUploadSession,
          UserProject, UploadFromOffset, UploadLimit, WithObjectMetadata,
          UploadContentLength, AutoFinalize, UploadBufferSize> {
 public:
  ResumableUploadRequest() = default;

  ResumableUploadRequest(std::string bucket_name, std::string object_name)
      : GenericObjectRequest(std::move(bucket_name), std::move(object_name)) {}
};

std::ostream& operator<<(std::ostream& os, ResumableUploadRequest const& r);

struct CreateResumableUploadResponse {
  static StatusOr<CreateResumableUploadResponse> FromHttpResponse(
      HttpResponse response);

  std::string upload_id;
};

bool operator==(CreateResumableUploadResponse const& lhs,
                CreateResumableUploadResponse const& rhs);
inline bool operator!=(CreateResumableUploadResponse const& lhs,
                       CreateResumableUploadResponse const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os,
                         CreateResumableUploadResponse const& r);

/**
 * A request to cancel a resumable upload.
 */
class DeleteResumableUploadRequest
    : public GenericRequest<DeleteResumableUploadRequest, UserProject> {
 public:
  DeleteResumableUploadRequest() = default;
  explicit DeleteResumableUploadRequest(std::string upload_session_url)
      : upload_session_url_(std::move(upload_session_url)) {}

  std::string const& upload_session_url() const { return upload_session_url_; }

 private:
  std::string upload_session_url_;
};

std::ostream& operator<<(std::ostream& os,
                         DeleteResumableUploadRequest const& r);

/**
 * A request to send one chunk in an upload session.
 */
class UploadChunkRequest
    : public GenericRequest<UploadChunkRequest, UserProject> {
 public:
  UploadChunkRequest() = default;

  // A non-final chunk
  UploadChunkRequest(std::string upload_session_url, std::uint64_t offset,
                     ConstBufferSequence payload)
      : upload_session_url_(std::move(upload_session_url)),
        offset_(offset),
        payload_(std::move(payload)) {}

  UploadChunkRequest(std::string upload_session_url, std::uint64_t offset,
                     ConstBufferSequence payload, HashValues full_object_hashes)
      : upload_session_url_(std::move(upload_session_url)),
        offset_(offset),
        upload_size_(offset + TotalBytes(payload)),
        payload_(std::move(payload)),
        full_object_hashes_(std::move(full_object_hashes)) {}

  std::string const& upload_session_url() const { return upload_session_url_; }
  std::uint64_t offset() const { return offset_; }
  absl::optional<std::uint64_t> upload_size() const { return upload_size_; }
  ConstBufferSequence const& payload() const { return payload_; }
  HashValues const& full_object_hashes() const { return full_object_hashes_; }

  bool last_chunk() const { return upload_size_.has_value(); }
  std::size_t payload_size() const { return TotalBytes(payload_); }
  std::string RangeHeader() const;
  std::string RangeHeaderValue() const;

  /**
   * Returns the request to continue writing at @p new_offset.
   *
   * @note the result of calling this with an out of range value is undefined
   *     behavior.
   */
  UploadChunkRequest RemainingChunk(std::uint64_t new_offset) const;

  // Chunks must be multiples of 256 KiB:
  //  https://cloud.google.com/storage/docs/json_api/v1/how-tos/resumable-upload
  static constexpr std::size_t kChunkSizeQuantum = 256 * 1024UL;

  static std::size_t RoundUpToQuantum(std::size_t max_chunk_size) {
    // If you are tempted to use bit manipulation to do this, modern compilers
    // known how to optimize this (coryan tested this at godbolt.org):
    //   https://godbolt.org/z/xxUsjg
    if (max_chunk_size % kChunkSizeQuantum == 0) {
      return max_chunk_size;
    }
    auto n = max_chunk_size / kChunkSizeQuantum;
    return (n + 1) * kChunkSizeQuantum;
  }

 private:
  std::string upload_session_url_;
  std::uint64_t offset_ = 0;
  absl::optional<std::uint64_t> upload_size_;
  ConstBufferSequence payload_;
  HashValues full_object_hashes_;
};

std::ostream& operator<<(std::ostream& os, UploadChunkRequest const& r);

/**
 * A request to query the status of a resumable upload.
 */
class QueryResumableUploadRequest
    : public GenericRequest<QueryResumableUploadRequest> {
 public:
  QueryResumableUploadRequest() = default;
  explicit QueryResumableUploadRequest(std::string upload_session_url)
      : upload_session_url_(std::move(upload_session_url)) {}

  std::string const& upload_session_url() const { return upload_session_url_; }

 private:
  std::string upload_session_url_;
};

std::ostream& operator<<(std::ostream& os,
                         QueryResumableUploadRequest const& r);

StatusOr<std::uint64_t> ParseRangeHeader(std::string const& range);

/**
 * The response from uploading a chunk and querying a resumable upload.
 *
 * We use the same type to represent the response for a UploadChunkRequest and a
 * QueryResumableUploadRequest because they are the same response.  Once a chunk
 * is successfully uploaded the response is the new status for the resumable
 * upload.
 */
struct QueryResumableUploadResponse {
  static StatusOr<QueryResumableUploadResponse> FromHttpResponse(
      HttpResponse response);
  QueryResumableUploadResponse() = default;
  QueryResumableUploadResponse(
      absl::optional<std::uint64_t> cs,
      absl::optional<google::cloud::storage::ObjectMetadata> p)
      : committed_size(std::move(cs)), payload(std::move(p)) {}
  QueryResumableUploadResponse(
      absl::optional<std::uint64_t> cs,
      absl::optional<google::cloud::storage::ObjectMetadata> p,
      std::multimap<std::string, std::string> rm)
      : committed_size(std::move(cs)),
        payload(std::move(p)),
        request_metadata(std::move(rm)) {}

  absl::optional<std::uint64_t> committed_size;
  absl::optional<google::cloud::storage::ObjectMetadata> payload;
  std::multimap<std::string, std::string> request_metadata;
};

bool operator==(QueryResumableUploadResponse const& lhs,
                QueryResumableUploadResponse const& rhs);
inline bool operator!=(QueryResumableUploadResponse const& lhs,
                       QueryResumableUploadResponse const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os,
                         QueryResumableUploadResponse const& r);

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_REQUESTS_H

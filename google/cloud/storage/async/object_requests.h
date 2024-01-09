// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OBJECT_REQUESTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OBJECT_REQUESTS_H

#include "google/cloud/storage/internal/async/connection_fwd.h"
#include "google/cloud/storage/internal/async/write_payload_fwd.h"
#include "google/cloud/storage/internal/grpc/make_cord.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/version.h"
#include "absl/strings/cord.h"
#include "absl/types/optional.h"
#include <cstdint>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * An opaque representation of the data for an object payload.
 */
class WritePayload {
 public:
  /// Creates an empty payload.
  WritePayload() = default;

  /// Creates a payload from @p p.
  explicit WritePayload(std::string p)
      : impl_(storage_internal::MakeCord(std::move(p))) {}

  /**
   * Creates a payload from @p p.
   *
   * @tparam T must be either:
   * - `char`, or
   * - `std::byte`, or
   * - an integer type that is *exactly* 8-bits wide (such as `std::int8_t`), or
   * - `std::string`, or
   * - `std::vector<U>`, where `U` is `char`, `std::byte`, or or an integer type
   *   that is exactly 8-bits wide
   *
   * @param p the resulting object takes ownership of the data in @p p.
   */
  template <typename T>
  explicit WritePayload(std::vector<T> p)
      : impl_(storage_internal::MakeCord(std::move(p))) {}

  /// Returns true if the payload has no data.
  bool empty() const { return impl_.empty(); }

  /// Returns the total size of the data.
  std::size_t size() const { return impl_.size(); }

  /**
   * Returns views into the data.
   *
   * Note that changing `*this` in any way (assignment, destruction, etc.)
   * invalidates all the returned buffers.
   */
  std::vector<absl::string_view> payload() const {
    return {impl_.chunk_begin(), impl_.chunk_end()};
  }

 private:
  friend struct storage_internal::WritePayloadImpl;
  explicit WritePayload(absl::Cord impl) : impl_(std::move(impl)) {}

  absl::Cord impl_;
};

/**
 * A request to insert object sans the data payload.
 *
 * This class can hold all the mandatory and optional parameters to insert an
 * object **except** for the data payload. The ideal representation for the data
 * payload depends on the type of request. For asynchronous requests the data
 * must be in an owning type, such as `WritePayload`. For blocking request, a
 * non-owning type (such as `absl::string_view`) can reduce data copying.
 *
 * This class is in the public API for the library because it is required for
 * mocking.
 */
class InsertObjectRequest {
 public:
  InsertObjectRequest() = default;
  InsertObjectRequest(std::string bucket_name, std::string object_name)
      : impl_(std::move(bucket_name), std::move(object_name)) {}

  std::string const& bucket_name() const { return impl_.bucket_name(); }
  std::string const& object_name() const { return impl_.object_name(); }

  template <typename... T>
  InsertObjectRequest& set_multiple_options(T&&... o) & {
    impl_.set_multiple_options(std::forward<T>(o)...);
    return *this;
  }
  template <typename... T>
  InsertObjectRequest&& set_multiple_options(T&&... o) && {
    return std::move(set_multiple_options(std::forward<T>(o)...));
  }

  template <typename T>
  bool HasOption() const {
    return impl_.HasOption<T>();
  }
  template <typename T>
  T GetOption() const {
    return impl_.GetOption<T>();
  }

 protected:
  struct Impl : public storage::internal::InsertObjectRequestImpl<Impl> {
    using storage::internal::InsertObjectRequestImpl<
        Impl>::InsertObjectRequestImpl;
  };
  Impl impl_;
};

/**
 * A request to start or resume a resumable upload.
 *
 * This class can hold all the mandatory and optional parameters to start or
 * resume a resumable upload. Resumable uploads can be used to stream large
 * objects as they can recover when the upload is interrupted. This request
 * does not contain any of the payload for the object; that is provided via a
 * `storage_experimental::AsyncWriter`.
 *
 * This class is in the public API for the library because it is required for
 * mocking.
 */
class ResumableUploadRequest {
 public:
  ResumableUploadRequest() = default;
  ResumableUploadRequest(std::string bucket_name, std::string object_name)
      : impl_(std::move(bucket_name), std::move(object_name)) {}

  std::string const& bucket_name() const { return impl_.bucket_name(); }
  std::string const& object_name() const { return impl_.object_name(); }

  template <typename... T>
  ResumableUploadRequest& set_multiple_options(T&&... o) & {
    impl_.set_multiple_options(std::forward<T>(o)...);
    return *this;
  }
  template <typename... T>
  ResumableUploadRequest&& set_multiple_options(T&&... o) && {
    return std::move(set_multiple_options(std::forward<T>(o)...));
  }

  template <typename T>
  bool HasOption() const {
    return impl_.HasOption<T>();
  }
  template <typename T>
  T GetOption() const {
    return impl_.GetOption<T>();
  }

 protected:
  friend class storage_internal::AsyncConnectionImpl;
  storage::internal::ResumableUploadRequest impl_;
};

/**
 * A request to read an object.
 *
 * This class can hold all the mandatory and optional parameters to read an
 * object.
 *
 * This class is in the public API for the library because it is required for
 * mocking.
 */
class ReadObjectRequest {
 public:
  ReadObjectRequest() = default;
  ReadObjectRequest(std::string bucket_name, std::string object_name)
      : impl_(std::move(bucket_name), std::move(object_name)) {}

  std::string const& bucket_name() const { return impl_.bucket_name(); }
  std::string const& object_name() const { return impl_.object_name(); }

  template <typename... T>
  ReadObjectRequest& set_multiple_options(T&&... o) & {
    impl_.set_multiple_options(std::forward<T>(o)...);
    return *this;
  }
  template <typename... T>
  ReadObjectRequest&& set_multiple_options(T&&... o) && {
    return std::move(set_multiple_options(std::forward<T>(o)...));
  }

  template <typename T>
  bool HasOption() const {
    return impl_.HasOption<T>();
  }
  template <typename T>
  T GetOption() const {
    return impl_.GetOption<T>();
  }

 protected:
  friend class storage_internal::AsyncConnectionImpl;
  storage::internal::ReadObjectRangeRequest impl_;
};

/**
 * A request to delete an object.
 *
 * This class can hold all the mandatory and optional parameters to delete an
 * object. This class is in the public API because it is required for mocking.
 */
class DeleteObjectRequest {
 public:
  DeleteObjectRequest() = default;
  DeleteObjectRequest(std::string bucket_name, std::string object_name)
      : impl_(std::move(bucket_name), std::move(object_name)) {}

  std::string const& bucket_name() const { return impl_.bucket_name(); }
  std::string const& object_name() const { return impl_.object_name(); }

  template <typename... T>
  DeleteObjectRequest& set_multiple_options(T&&... o) & {
    impl_.set_multiple_options(std::forward<T>(o)...);
    return *this;
  }
  template <typename... T>
  DeleteObjectRequest&& set_multiple_options(T&&... o) && {
    return std::move(set_multiple_options(std::forward<T>(o)...));
  }

  template <typename T>
  bool HasOption() const {
    return impl_.HasOption<T>();
  }
  template <typename T>
  T GetOption() const {
    return impl_.GetOption<T>();
  }

 protected:
  friend class storage_internal::AsyncConnectionImpl;
  storage::internal::DeleteObjectRequest impl_;
};

/**
 * A request to compose multiple objects into a single object.
 *
 * This class can hold all the mandatory and optional parameters to compose
 * objects. This class is in the public API because it is required for mocking.
 */
class ComposeObjectRequest {
 public:
  ComposeObjectRequest() = default;
  ComposeObjectRequest(std::string bucket_name,
                       std::vector<storage::ComposeSourceObject> source_objects,
                       std::string destination_object_name)
      : impl_(std::move(bucket_name), std::move(source_objects),
              std::move(destination_object_name)) {}

  std::string const& bucket_name() const { return impl_.bucket_name(); }
  std::string const& object_name() const { return impl_.object_name(); }
  std::vector<storage::ComposeSourceObject> source_objects() const {
    return impl_.source_objects();
  }

  template <typename... T>
  ComposeObjectRequest& set_multiple_options(T&&... o) & {
    impl_.set_multiple_options(std::forward<T>(o)...);
    return *this;
  }
  template <typename... T>
  ComposeObjectRequest&& set_multiple_options(T&&... o) && {
    return std::move(set_multiple_options(std::forward<T>(o)...));
  }

  template <typename T>
  bool HasOption() const {
    return impl_.HasOption<T>();
  }
  template <typename T>
  T GetOption() const {
    return impl_.GetOption<T>();
  }

 protected:
  friend class storage_internal::AsyncConnectionImpl;
  storage::internal::ComposeObjectRequest impl_;
};

/**
 * A request to compose multiple objects into a single object.
 *
 * This class can hold all the mandatory and optional parameters to compose
 * objects. This class is in the public API because it is required for mocking.
 */
class RewriteObjectRequest {
 public:
  RewriteObjectRequest() = default;
  RewriteObjectRequest(std::string source_bucket, std::string source_object,
                       std::string destination_bucket,
                       std::string destination_object)
      : impl_(std::move(source_bucket), std::move(source_object),
              std::move(destination_bucket), std::move(destination_object),
              /*rewrite_token=*/std::string{}) {}

  std::string const& source_bucket() const { return impl_.source_bucket(); }
  std::string const& source_object() const { return impl_.source_object(); }
  std::string const& destination_bucket() const {
    return impl_.destination_bucket();
  }
  std::string const& destination_object() const {
    return impl_.destination_object();
  }
  std::string const& rewrite_token() const { return impl_.rewrite_token(); }
  RewriteObjectRequest& set_rewrite_token(std::string t) & {
    impl_.set_rewrite_token(std::move(t));
    return *this;
  }
  RewriteObjectRequest&& set_rewrite_token(std::string t) && {
    return std::move(set_rewrite_token(std::move(t)));
  }

  template <typename... T>
  RewriteObjectRequest& set_multiple_options(T&&... o) & {
    impl_.set_multiple_options(std::forward<T>(o)...);
    return *this;
  }
  template <typename... T>
  RewriteObjectRequest&& set_multiple_options(T&&... o) && {
    return std::move(set_multiple_options(std::forward<T>(o)...));
  }

  template <typename T>
  bool HasOption() const {
    return impl_.HasOption<T>();
  }
  template <typename T>
  T GetOption() const {
    return impl_.GetOption<T>();
  }

 protected:
  friend class storage_internal::AsyncConnectionImpl;
  storage::internal::RewriteObjectRequest impl_;
};

/**
 * The result of a (maybe partial) object rewrite.
 *
 * Object rewrites may require one or more requests. The final request returns
 * the rewritten object metadata. Previous requests return the number of bytes
 * rewritten and any
 */
struct RewriteObjectResponse {
  std::int64_t total_bytes_rewritten;
  std::int64_t object_size;
  std::string rewrite_token;
  absl::optional<storage::ObjectMetadata> metadata;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OBJECT_REQUESTS_H

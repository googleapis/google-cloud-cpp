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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_REWRITER_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_REWRITER_H_

#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/storage/internal/raw_client.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Represents the status of a rewrite operation.
 *
 * The ObjectRewrite class may require multiple calls to `Iterate()` to finish
 * the copy. This class represents the progress in a partially completed
 * rewrite. Applications can use this information to inform users of the
 * progress and the expected completion time.
 */
struct RewriteProgress {
  std::uint64_t total_bytes_rewritten;
  std::uint64_t object_size;
  bool done;
};

/**
 * Complete long running object rewrite operations.
 *
 * The `Client::RewriteObject()` operation allows applications to copy objects
 * across location boundaries, and to rewrite objects with different encryption
 * keys. In some circumstances it may take multiple calls to the service to
 * complete a rewrite, this class encapsulates the state of a partial copy.
 */
class ObjectRewriter {
 public:
  ObjectRewriter(std::shared_ptr<internal::RawClient> client,
                 internal::RewriteObjectRequest request);

  /**
   * Perform one iteration in the rewrite.
   *
   * @return The progress after the iteration. If the rewrite has completed the
   *   application can use `Result()` to examine the metadata for the newly
   *   created object.
   */
  StatusOr<RewriteProgress> Iterate();

  /// The current progress on the rewrite operation.
  StatusOr<RewriteProgress> CurrentProgress() const {
    if (not last_error_.ok()) {
      return last_error_;
    }
    return progress_;
  }

  /**
   * Iterate until the operation completes using a callback to report progress.
   *
   * @note This operation blocks until the copy is finished. For very large
   * objects that could take substantial time. Applications may need to persist
   * the rewrite operation. Some applications may want to wrap this call with
   * `std::async`, and run the copy on a separate thread.
   *
   * @return the object metadata once the copy completes.
   */
  StatusOr<ObjectMetadata> Result() {
    return ResultWithProgressCallback([](StatusOr<RewriteProgress> const&) {});
  }

  /**
   * Iterate until the operation completes using a callback to report progress.
   *
   * @note This operation blocks until the copy is finished. For very large
   * objects that could take substantial time. Applications may need to persist
   * the rewrite operation. Some applications may want to wrap this call with
   * `std::async`, and run the copy on a separate thread.
   *
   * @param cb the callback object.
   *
   * @tparam Functor the type of the callback object. It must satisfy:
   *   `std:is_invocable<Functor, StatusOr<RewriteProgress>>:: value == true`.
   *
   * @return the object metadata once the copy completes.
   */
  template <
      typename Functor,
      typename std::enable_if<google::cloud::internal::is_invocable<
                                  Functor, StatusOr<RewriteProgress>>::value,
                              int>::type = 0>
  StatusOr<ObjectMetadata> ResultWithProgressCallback(Functor cb) {
    while (not progress_.done) {
      cb(Iterate());
    }
    if (not last_error_.ok()) {
      return last_error_;
    }
    return result_;
  }

  /**
   * The current rewrite token.
   *
   * Applications can save the token of partially completed rewrites, and
   * restart those operations using `Client::CopyObjectRestart`, even if the
   * application has terminated. It is up to the application to preserve
   * all the other information for the request, including source and destination
   * buckets, encryption keys, and any preconditions affecting the request.
   *
   * @note For rewrites that have not started the token is an empty string.
   *   For rewrites that have completed the token is also an empty string. The
   *   application should preserve other information (such as the
   *   `RewriteProgress`) to avoid repeating a rewrite.
   */
  std::string const& token() const { return request_.rewrite_token(); }

 private:
  std::shared_ptr<internal::RawClient> client_;
  internal::RewriteObjectRequest request_;
  RewriteProgress progress_;
  ObjectMetadata result_;
  Status last_error_;
};
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_REWRITER_H_

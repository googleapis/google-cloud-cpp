// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_BUCKET_NAME_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_BUCKET_NAME_H

#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include <iosfwd>
#include <string>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Represent bucket names.
 *
 * Bucket names must be properly formatted before using the storage gRPC API.
 *
 * @note This class makes no effort to validate the bucket id. The service
 *     imposes some restrictions on [bucket names], such as length and the
 *     character set. The application must ensure the bucket name meets these
 *     restrictions, as this class performs no validation on its inputs.
 *
 * [bucket names]: https://cloud.google.com/storage/docs/buckets#naming
 */
class BucketName {
 public:
  /// Constructs a bucket name using @p name.
  explicit BucketName(std::string name);

  /// @name Copy and move
  ///@{
  BucketName(BucketName const&) = default;
  BucketName& operator=(BucketName const&) = default;
  BucketName(BucketName&&) = default;
  BucketName& operator=(BucketName&&) = default;

  /// Returns the name.
  std::string const& name() const { return name_; }

  /**
   * Returns the fully qualified bucket name as a string of the form:
   * "projects/_/buckets/{name}"
   */
  std::string FullName() const;

  /// @name Equality operators
  ///@{
  friend bool operator==(BucketName const& a, BucketName const& b);
  friend bool operator!=(BucketName const& a, BucketName const& b);
  ///@}

  /// Output the `FullName()` format.
  friend std::ostream& operator<<(std::ostream&, BucketName const&);

 private:
  std::string name_;
};

/**
 * Constructs a `BucketName` from the given @p full_name.
 * Returns a non-OK Status if `full_name` is improperly formed.
 */
StatusOr<BucketName> MakeBucketName(absl::string_view full_name);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_BUCKET_NAME_H

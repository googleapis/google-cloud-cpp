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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_FUNCTION_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_FUNCTION_IMPL_H

#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/storage/version.h"
#include "absl/types/optional.h"
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * A function that does not validate.
 */
class NullHashFunction : public HashFunction {
 public:
  NullHashFunction() = default;

  std::string Name() const override;
  void Update(absl::string_view) override;
  Status Update(std::int64_t offset, absl::string_view buffer) override;
  Status Update(std::int64_t offset, absl::string_view buffer,
                std::uint32_t buffer_crc) override;
  Status Update(std::int64_t offset, absl::Cord const& buffer,
                std::uint32_t buffer_crc) override;
  HashValues Finish() override;
};

/**
 * A composite function.
 */
class CompositeFunction : public HashFunction {
 public:
  CompositeFunction(std::unique_ptr<HashFunction> a,
                    std::unique_ptr<HashFunction> b)
      : a_(std::move(a)), b_(std::move(b)) {}

  std::string Name() const override;
  void Update(absl::string_view buffer) override;
  Status Update(std::int64_t offset, absl::string_view buffer) override;
  Status Update(std::int64_t offset, absl::string_view buffer,
                std::uint32_t buffer_crc) override;
  Status Update(std::int64_t offset, absl::Cord const& buffer,
                std::uint32_t buffer_crc) override;
  HashValues Finish() override;

 private:
  std::unique_ptr<HashFunction> a_;
  std::unique_ptr<HashFunction> b_;
};

/**
 * A function based on MD5 hashes.
 */
class MD5HashFunction : public HashFunction {
 public:
  MD5HashFunction() = default;

  MD5HashFunction(MD5HashFunction const&) = delete;
  MD5HashFunction& operator=(MD5HashFunction const&) = delete;

  static std::unique_ptr<MD5HashFunction> Create();

  std::string Name() const override { return "md5"; }
  void Update(absl::string_view buffer) override = 0;
  Status Update(std::int64_t offset, absl::string_view buffer) override;
  Status Update(std::int64_t offset, absl::string_view buffer,
                std::uint32_t buffer_crc) override;
  Status Update(std::int64_t offset, absl::Cord const& buffer,
                std::uint32_t buffer_crc) override;
  HashValues Finish() override;

 protected:
  // (8 bits per byte) * 16 bytes = 128 bits
  using Hash = std::array<std::uint8_t, 16>;
  virtual Hash FinishImpl() = 0;

 private:
  std::int64_t minimum_offset_ = 0;
  absl::optional<HashValues> hashes_;
};

/**
 * A function based on CRC32C checksums.
 */
class Crc32cHashFunction : public HashFunction {
 public:
  Crc32cHashFunction() = default;

  Crc32cHashFunction(Crc32cHashFunction const&) = delete;
  Crc32cHashFunction& operator=(Crc32cHashFunction const&) = delete;

  std::string Name() const override { return "crc32c"; }
  void Update(absl::string_view buffer) override;
  Status Update(std::int64_t offset, absl::string_view buffer) override;
  Status Update(std::int64_t offset, absl::string_view buffer,
                std::uint32_t buffer_crc) override;
  Status Update(std::int64_t offset, absl::Cord const& buffer,
                std::uint32_t buffer_crc) override;
  HashValues Finish() override;

 private:
  std::uint32_t current_ = 0;
  std::int64_t minimum_offset_ = 0;
};

/**
 * A hash function returning a pre-computed hash.
 */
class PrecomputedHashFunction : public HashFunction {
 public:
  explicit PrecomputedHashFunction(HashValues p)
      : precomputed_hash_(std::move(p)) {}

  PrecomputedHashFunction(PrecomputedHashFunction const&) = delete;
  PrecomputedHashFunction& operator=(PrecomputedHashFunction const&) = delete;

  std::string Name() const override;
  void Update(absl::string_view buffer) override;
  Status Update(std::int64_t offset, absl::string_view buffer) override;
  Status Update(std::int64_t offset, absl::string_view buffer,
                std::uint32_t buffer_crc) override;
  Status Update(std::int64_t offset, absl::Cord const& buffer,
                std::uint32_t buffer_crc) override;
  HashValues Finish() override;

 private:
  HashValues precomputed_hash_;
};

/**
 * Validates per-message CRC32C checksums and delegates the full hashing
 * computation.
 *
 * When performing downloads over gRPC the payload has per-message CRC32C
 * checksums. We want to validate these checksums as the data is downloaded. The
 * service may also return full object checksums. We can compose the per-message
 * checksums to compute the full object checksums and validate this against the
 * returned values. When the download range is not a full object download we do
 * not want to compute the range checksum because the service will not return
 * a value, so there is nothing to compare.
 *
 * Composing this class with a normal `Crc32cHashFunction` works well for full
 * downloads. This class validates each message, the `Crc32cHashFunction`
 * composes the checksums without reading all the data again.
 *
 * Composing this class with `NullHashFunction` works well for partial
 * downloads. This class validates each message, and we do not waste CPU trying
 * to compute the checksum for the partial download.
 */
class Crc32cMessageHashFunction : public HashFunction {
 public:
  explicit Crc32cMessageHashFunction(std::unique_ptr<HashFunction> child)
      : child_(std::move(child)) {}

  Crc32cMessageHashFunction(Crc32cMessageHashFunction const&) = delete;
  Crc32cMessageHashFunction& operator=(Crc32cMessageHashFunction const&) =
      delete;

  std::string Name() const override;
  void Update(absl::string_view buffer) override;
  Status Update(std::int64_t offset, absl::string_view buffer) override;
  Status Update(std::int64_t offset, absl::string_view buffer,
                std::uint32_t buffer_crc) override;
  Status Update(std::int64_t offset, absl::Cord const& buffer,
                std::uint32_t buffer_crc) override;
  HashValues Finish() override;

 private:
  std::unique_ptr<HashFunction> child_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_FUNCTION_IMPL_H

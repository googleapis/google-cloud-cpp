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

#include "google/cloud/storage/internal/hash_function_impl.h"
#include "google/cloud/storage/internal/base64.h"
#include "google/cloud/storage/internal/crc32c.h"
#include "google/cloud/internal/big_endian.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::internal::InvalidArgumentError;
using ::google::cloud::storage_internal::Crc32c;
using ::google::cloud::storage_internal::ExtendCrc32c;

template <typename Buffer>
bool AlreadyHashed(std::int64_t offset, Buffer const& buffer,
                   std::int64_t minimum_offset) {
  // TODO(#14566) - maybe this is more forgiving than we want to be
  auto const end = offset + buffer.size();
  return static_cast<std::int64_t>(end) <= minimum_offset;
}

}  // namespace

std::string NullHashFunction::Name() const { return "null"; }

void NullHashFunction::Update(absl::string_view) {}

Status NullHashFunction::Update(std::int64_t /*offset*/,
                                absl::string_view /*buffer*/) {
  return {};
}

Status NullHashFunction::Update(std::int64_t /*offset*/,
                                absl::string_view /*buffer*/,
                                std::uint32_t /*buffer_crc*/) {
  return {};
}

Status NullHashFunction::Update(std::int64_t /*offset*/,
                                absl::Cord const& /*buffer*/,
                                std::uint32_t /*buffer_crc*/) {
  return {};
}

HashValues NullHashFunction::Finish() { return HashValues{}; }

std::string CompositeFunction::Name() const {
  return "composite(" + a_->Name() + "," + b_->Name() + ")";
}

void CompositeFunction::Update(absl::string_view buffer) {
  a_->Update(buffer);
  b_->Update(buffer);
}

Status CompositeFunction::Update(std::int64_t offset,
                                 absl::string_view buffer) {
  auto status = a_->Update(offset, buffer);
  if (!status.ok()) return status;
  return b_->Update(offset, buffer);
}

Status CompositeFunction::Update(std::int64_t offset, absl::string_view buffer,
                                 std::uint32_t buffer_crc) {
  auto status = a_->Update(offset, buffer, buffer_crc);
  if (!status.ok()) return status;
  return b_->Update(offset, buffer, buffer_crc);
}

Status CompositeFunction::Update(std::int64_t offset, absl::Cord const& buffer,
                                 std::uint32_t buffer_crc) {
  auto status = a_->Update(offset, buffer, buffer_crc);
  if (!status.ok()) return status;
  return b_->Update(offset, buffer, buffer_crc);
}

HashValues CompositeFunction::Finish() {
  return Merge(a_->Finish(), b_->Finish());
}

Status MD5HashFunction::Update(std::int64_t offset, absl::string_view buffer) {
  if (offset == minimum_offset_ || minimum_offset_ == 0) {
    Update(buffer);
    minimum_offset_ = offset + buffer.size();
    return {};
  }
  if (AlreadyHashed(offset, buffer, minimum_offset_)) return {};
  return InvalidArgumentError("mismatched offset", GCP_ERROR_INFO());
}

Status MD5HashFunction::Update(std::int64_t offset, absl::string_view buffer,
                               std::uint32_t /*buffer_crc*/) {
  return Update(offset, buffer);
}

Status MD5HashFunction::Update(std::int64_t offset, absl::Cord const& buffer,
                               std::uint32_t /*buffer_crc*/) {
  if (offset == minimum_offset_ || minimum_offset_ == 0) {
    for (auto i = buffer.chunk_begin(); i != buffer.chunk_end(); ++i) {
      Update(*i);
    }
    minimum_offset_ = offset + buffer.size();
    return {};
  }
  if (AlreadyHashed(offset, buffer, minimum_offset_)) return {};
  return InvalidArgumentError("mismatched offset", GCP_ERROR_INFO());
}

HashValues MD5HashFunction::Finish() {
  if (hashes_.has_value()) return *hashes_;
  Hash hash = FinishImpl();
  hashes_ = HashValues{/*.crc32c=*/{}, /*.md5=*/Base64Encode(hash)};
  return *hashes_;
}

void Crc32cHashFunction::Update(absl::string_view buffer) {
  current_ = ExtendCrc32c(current_, buffer);
}

Status Crc32cHashFunction::Update(std::int64_t offset,
                                  absl::string_view buffer) {
  if (offset == minimum_offset_ || minimum_offset_ == 0) {
    Update(buffer);
    minimum_offset_ = offset + buffer.size();
    return {};
  }
  if (AlreadyHashed(offset, buffer, minimum_offset_)) return {};
  return InvalidArgumentError("mismatched offset", GCP_ERROR_INFO());
}

Status Crc32cHashFunction::Update(std::int64_t offset, absl::string_view buffer,
                                  std::uint32_t buffer_crc) {
  if (offset == minimum_offset_ || minimum_offset_ == 0) {
    current_ = ExtendCrc32c(current_, buffer, buffer_crc);
    minimum_offset_ = offset + buffer.size();
    return {};
  }
  if (AlreadyHashed(offset, buffer, minimum_offset_)) return {};
  return InvalidArgumentError("mismatched offset", GCP_ERROR_INFO());
}

Status Crc32cHashFunction::Update(std::int64_t offset, absl::Cord const& buffer,
                                  std::uint32_t buffer_crc) {
  if (offset == minimum_offset_ || minimum_offset_ == 0) {
    current_ = ExtendCrc32c(current_, buffer, buffer_crc);
    minimum_offset_ = offset + buffer.size();
    return {};
  }
  if (AlreadyHashed(offset, buffer, minimum_offset_)) return {};
  return InvalidArgumentError("mismatched offset", GCP_ERROR_INFO());
}

HashValues Crc32cHashFunction::Finish() {
  std::string const hash = google::cloud::internal::EncodeBigEndian(current_);
  return HashValues{/*.crc32c=*/Base64Encode(hash), /*.md5=*/{}};
}

std::string PrecomputedHashFunction::Name() const {
  return "precomputed(" + Format(precomputed_hash_) + ")";
}

void PrecomputedHashFunction::Update(absl::string_view /*buffer*/) {}

Status PrecomputedHashFunction::Update(std::int64_t /*offset*/,
                                       absl::string_view /*buffer*/) {
  return Status{};
}

Status PrecomputedHashFunction::Update(std::int64_t /*offset*/,
                                       absl::string_view /*buffer*/,
                                       std::uint32_t /*buffer_crc*/) {
  return Status{};
}

Status PrecomputedHashFunction::Update(std::int64_t /*offset*/,
                                       absl::Cord const& /*buffer*/,
                                       std::uint32_t /*buffer_crc*/) {
  return Status{};
}

HashValues PrecomputedHashFunction::Finish() { return precomputed_hash_; }

std::string Crc32cMessageHashFunction::Name() const {
  return "crc32c-message(" + child_->Name() + ")";
}

void Crc32cMessageHashFunction::Update(absl::string_view buffer) {
  return child_->Update(buffer);
}

Status Crc32cMessageHashFunction::Update(std::int64_t offset,
                                         absl::string_view buffer) {
  return child_->Update(offset, buffer);
}

Status Crc32cMessageHashFunction::Update(std::int64_t offset,
                                         absl::string_view buffer,
                                         std::uint32_t buffer_crc) {
  if (Crc32c(buffer) != buffer_crc) {
    // No need to update the child, this is an error that should stop any
    // download.
    return InvalidArgumentError("mismatched crc32c checksum", GCP_ERROR_INFO());
  }
  return child_->Update(offset, buffer, buffer_crc);
}

Status Crc32cMessageHashFunction::Update(std::int64_t offset,
                                         absl::Cord const& buffer,
                                         std::uint32_t buffer_crc) {
  if (Crc32c(buffer) != buffer_crc) {
    // No need to update the child, this is an error that should stop any
    // download.
    return InvalidArgumentError("mismatched crc32c checksum", GCP_ERROR_INFO());
  }
  return child_->Update(offset, buffer, buffer_crc);
}

HashValues Crc32cMessageHashFunction::Finish() { return child_->Finish(); }

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

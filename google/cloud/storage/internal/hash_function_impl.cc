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
#include "google/cloud/storage/internal/crc32c.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/internal/big_endian.h"
#include "google/cloud/internal/make_status.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::internal::InvalidArgumentError;
using ::google::cloud::storage_internal::ExtendCrc32c;

using ContextPtr = std::unique_ptr<EVP_MD_CTX, MD5HashFunction::ContextDeleter>;

ContextPtr CreateDigestCtx() {
// The name of the function to create and delete EVP_MD_CTX objects changed
// with OpenSSL 1.1.0.
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)  // Older than version 1.1.0.
  return ContextPtr(EVP_MD_CTX_create());
#else
  return ContextPtr(EVP_MD_CTX_new());
#endif
}

void DeleteDigestCtx(EVP_MD_CTX* context) {
// The name of the function to free an EVP_MD_CTX changed in OpenSSL 1.1.0.
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)  // Older than version 1.1.0.
  EVP_MD_CTX_destroy(context);
#else
  EVP_MD_CTX_free(context);
#endif  // OPENSSL_VERSION_NUMBER >= 0x10100000L
}

template <typename Buffer>
bool AlreadyHashed(std::int64_t offset, Buffer const& buffer,
                   std::int64_t minimum_offset) {
  auto const end = offset + buffer.size();
  return static_cast<std::int64_t>(end) < minimum_offset;
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
  return Merge(std::move(*a_).Finish(), std::move(*b_).Finish());
}

MD5HashFunction::MD5HashFunction() : impl_(CreateDigestCtx()) {
  EVP_DigestInit_ex(impl_.get(), EVP_md5(), nullptr);
}

void MD5HashFunction::Update(absl::string_view buffer) {
  EVP_DigestUpdate(impl_.get(), buffer.data(), buffer.size());
}

Status MD5HashFunction::Update(std::int64_t offset, absl::string_view buffer) {
  if (offset == minimum_offset_) {
    Update(buffer);
    minimum_offset_ += buffer.size();
    return {};
  }
  if (AlreadyHashed(offset, buffer, minimum_offset_)) return {};
  return InvalidArgumentError("mismatched offset", GCP_ERROR_INFO());
}

Status MD5HashFunction::Update(std::int64_t offset, absl::string_view buffer,
                               std::uint32_t /*buffer_crc*/) {
  if (offset == minimum_offset_) {
    Update(buffer);
    minimum_offset_ += buffer.size();
    return {};
  }
  if (AlreadyHashed(offset, buffer, minimum_offset_)) return {};
  return InvalidArgumentError("mismatched offset", GCP_ERROR_INFO());
}

Status MD5HashFunction::Update(std::int64_t offset, absl::Cord const& buffer,
                               std::uint32_t /*buffer_crc*/) {
  if (offset == minimum_offset_) {
    for (auto i = buffer.chunk_begin(); i != buffer.chunk_end(); ++i) {
      Update(*i);
    }
    minimum_offset_ += buffer.size();
    return {};
  }
  if (AlreadyHashed(offset, buffer, minimum_offset_)) return {};
  return InvalidArgumentError("mismatched offset", GCP_ERROR_INFO());
}

HashValues MD5HashFunction::Finish() {
  if (hashes_.has_value()) return *hashes_;
  std::vector<std::uint8_t> hash(EVP_MD_size(EVP_md5()));
  unsigned int len = 0;
  // Note: EVP_DigestFinal_ex() consumes an `unsigned char*` for the output
  // array.  On some platforms (read PowerPC and ARM), the default `char` is
  // unsigned. In those platforms it is possible that
  // `std::uint8_t != unsigned char` and the `reinterpret_cast<>` is needed. It
  // should be safe in any case.
  EVP_DigestFinal_ex(impl_.get(), reinterpret_cast<unsigned char*>(hash.data()),
                     &len);
  hash.resize(len);
  hashes_ = HashValues{/*.crc32c=*/{}, /*.md5=*/Base64Encode(hash)};
  return *hashes_;
}

void MD5HashFunction::ContextDeleter::operator()(EVP_MD_CTX* context) {
  DeleteDigestCtx(context);
}

void Crc32cHashFunction::Update(absl::string_view buffer) {
  current_ = ExtendCrc32c(current_, buffer);
}

Status Crc32cHashFunction::Update(std::int64_t offset,
                                  absl::string_view buffer) {
  if (offset == minimum_offset_) {
    Update(buffer);
    minimum_offset_ += buffer.size();
    return {};
  }
  if (AlreadyHashed(offset, buffer, minimum_offset_)) return {};
  return InvalidArgumentError("mismatched offset", GCP_ERROR_INFO());
}

Status Crc32cHashFunction::Update(std::int64_t offset, absl::string_view buffer,
                                  std::uint32_t buffer_crc) {
  if (offset == minimum_offset_) {
    current_ = ExtendCrc32c(current_, buffer, buffer_crc);
    minimum_offset_ += buffer.size();
    return {};
  }
  if (AlreadyHashed(offset, buffer, minimum_offset_)) return {};
  return InvalidArgumentError("mismatched offset", GCP_ERROR_INFO());
}

Status Crc32cHashFunction::Update(std::int64_t offset, absl::Cord const& buffer,
                                  std::uint32_t buffer_crc) {
  if (offset == minimum_offset_) {
    current_ = ExtendCrc32c(current_, buffer, buffer_crc);
    minimum_offset_ += buffer.size();
    return {};
  }
  if (AlreadyHashed(offset, buffer, minimum_offset_)) return {};
  return InvalidArgumentError("mismatched offset", GCP_ERROR_INFO());
}

HashValues Crc32cHashFunction::Finish() {
  std::string const hash = google::cloud::internal::EncodeBigEndian(current_);
  return HashValues{/*.crc32c=*/Base64Encode(hash), /*.md5=*/{}};
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

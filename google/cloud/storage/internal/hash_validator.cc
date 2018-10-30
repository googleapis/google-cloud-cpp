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

#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/internal/big_endian.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/status.h"
#include <crc32c/crc32c.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

void HashValidator::CheckResult(std::string const& msg,
                                HashValidator::Result const& result) {
  if (not result.is_mismatch) {
    return;
  }
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // Sometimes the server simply does not have a MD5 hash to send us, the most
  // common case is a composed object, particularly one formed from encrypted
  // components, where computing the MD5 would require decrypting and re-reading
  // all the components. In that case we just do not raise an exception even if
  // there is a mismatch.
  throw HashMismatchError(msg, result.received, result.computed);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

HashValidator::Result HashValidator::FinishAndCheck(std::string const& msg,
                                                    HashValidator&& validator) {
  auto result = std::move(validator).Finish();
  CheckResult(msg, result);
  return result;
}

void CompositeValidator::Update(std::string const& payload) {
  left_->Update(payload);
  right_->Update(payload);
}

void CompositeValidator::ProcessMetadata(ObjectMetadata const& meta) {
  left_->ProcessMetadata(meta);
  right_->ProcessMetadata(meta);
}

void CompositeValidator::ProcessHeader(std::string const& key,
                                       std::string const& value) {
  left_->ProcessHeader(key, value);
  right_->ProcessHeader(key, value);
}

HashValidator::Result CompositeValidator::Finish() && {
  auto left_result = std::move(*left_).Finish();
  auto right_result = std::move(*right_).Finish();

  auto received = left_->Name() + "=" + left_result.received;
  received += "," + right_->Name() + "=" + right_result.received;
  auto computed = left_->Name() + "=" + left_result.computed;
  computed += "," + right_->Name() + "=" + right_result.computed;
  bool is_mismatch = left_result.is_mismatch or right_result.is_mismatch;
  return Result{std::move(received), std::move(computed), is_mismatch};
}

MD5HashValidator::MD5HashValidator() : context_{} { MD5_Init(&context_); }

void MD5HashValidator::Update(std::string const& payload) {
  MD5_Update(&context_, payload.c_str(), payload.size());
}

void MD5HashValidator::ProcessMetadata(ObjectMetadata const& meta) {
  if (meta.md5_hash().empty()) {
    // When using the XML API the metadata is empty, but the headers are not. In
    // that case we do not want to replace the received hash with an empty
    // value.
    return;
  }
  received_hash_ = meta.md5_hash();
}

void MD5HashValidator::ProcessHeader(std::string const& key,
                                     std::string const& value) {
  if (key != "x-goog-hash") {
    return;
  }
  auto pos = value.find("md5=");
  if (pos == std::string::npos) {
    return;
  }
  auto end = value.find(',', pos);
  if (end == std::string::npos) {
    received_hash_ = value.substr(pos + 4);
    return;
  }
  received_hash_ = value.substr(pos + 4, end - pos - 4);
}

HashValidator::Result MD5HashValidator::Finish() && {
  std::string hash(MD5_DIGEST_LENGTH, ' ');
  MD5_Final(reinterpret_cast<unsigned char*>(&hash[0]), &context_);
  auto computed = OpenSslUtils::Base64Encode(hash);
  bool is_mismatch =
      not received_hash_.empty() and (received_hash_ != computed);
  return Result{std::move(received_hash_), std::move(computed), is_mismatch};
}

Crc32cHashValidator::Crc32cHashValidator() : current_(0) {}

void Crc32cHashValidator::Update(std::string const& payload) {
  current_ = crc32c::Extend(
      current_, reinterpret_cast<std::uint8_t const*>(payload.data()),
      payload.size());
}

void Crc32cHashValidator::ProcessMetadata(ObjectMetadata const& meta) {
  if (meta.crc32c().empty()) {
    // When using the XML API the metadata is empty, but the headers are not. In
    // that case we do not want to replace the received hash with an empty
    // value.
    return;
  }
  received_hash_ = meta.crc32c();
}

void Crc32cHashValidator::ProcessHeader(std::string const& key,
                                        std::string const& value) {
  if (key != "x-goog-hash") {
    return;
  }
  auto pos = value.find("crc32c=");
  if (pos == std::string::npos) {
    return;
  }
  auto end = value.find(',', pos);
  if (end == std::string::npos) {
    received_hash_ = value.substr(pos + 7);
    return;
  }
  received_hash_ = value.substr(pos + 7, end - pos - 7);
}

HashValidator::Result Crc32cHashValidator::Finish() && {
  std::uint32_t big_endian = google::cloud::internal::ToBigEndian(current_);
  std::string hash;
  hash.resize(sizeof(big_endian));
  std::memcpy(&hash[0], &big_endian, sizeof(big_endian));
  auto computed = OpenSslUtils::Base64Encode(hash);
  bool is_mismatch =
      not received_hash_.empty() and (received_hash_ != computed);
  return Result{std::move(received_hash_), std::move(computed), is_mismatch};
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

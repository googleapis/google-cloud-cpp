// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/hash_function_impl.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/internal/big_endian.h"
#include "absl/memory/memory.h"
#include <crc32c/crc32c.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

std::string CompositeFunction::Name() const {
  return "composite(" + a_->Name() + "," + b_->Name() + ")";
}

void CompositeFunction::Update(char const* buf, std::size_t n) {
  a_->Update(buf, n);
  b_->Update(buf, n);
}

HashValues CompositeFunction::Finish() && {
  return Merge(std::move(*a_).Finish(), std::move(*b_).Finish());
}

MD5HashFunction::MD5HashFunction() : context_{} { MD5_Init(&context_); }

void MD5HashFunction::Update(char const* buf, std::size_t n) {
  MD5_Update(&context_, buf, n);
}

HashValues MD5HashFunction::Finish() && {
  std::vector<std::uint8_t> hash(MD5_DIGEST_LENGTH, ' ');
  MD5_Final(reinterpret_cast<unsigned char*>(hash.data()), &context_);
  return HashValues{/*.crc32c=*/{}, /*.md5=*/Base64Encode(hash)};
}

void Crc32cHashFunction::Update(char const* buf, std::size_t n) {
  current_ =
      crc32c::Extend(current_, reinterpret_cast<std::uint8_t const*>(buf), n);
}

HashValues Crc32cHashFunction::Finish() && {
  std::string const hash = google::cloud::internal::EncodeBigEndian(current_);
  return HashValues{/*.crc32c=*/Base64Encode(hash), /*.md5=*/{}};
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

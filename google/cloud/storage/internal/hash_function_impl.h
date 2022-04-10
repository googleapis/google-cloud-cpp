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
#include <openssl/evp.h>
#include <map>
#include <memory>
#include <string>

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

  std::string Name() const override { return "null"; }
  void Update(char const*, std::size_t) override {}
  HashValues Finish() && override { return HashValues{}; }
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
  void Update(char const* buf, std::size_t n) override;
  HashValues Finish() && override;

 private:
  std::unique_ptr<HashFunction> a_;
  std::unique_ptr<HashFunction> b_;
};

/**
 * A function based on MD5 hashes.
 */
class MD5HashFunction : public HashFunction {
 public:
  MD5HashFunction();

  MD5HashFunction(MD5HashFunction const&) = delete;
  MD5HashFunction& operator=(MD5HashFunction const&) = delete;

  std::string Name() const override { return "md5"; }
  void Update(char const* buf, std::size_t n) override;
  HashValues Finish() && override;

  struct ContextDeleter {
    void operator()(EVP_MD_CTX*);
  };

 private:
  std::unique_ptr<EVP_MD_CTX, ContextDeleter> impl_;
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
  void Update(char const* buf, std::size_t n) override;
  HashValues Finish() && override;

 private:
  std::uint32_t current_{0};
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_FUNCTION_IMPL_H

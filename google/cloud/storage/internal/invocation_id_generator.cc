// Copyright 2022 Google LLC
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

#include "google/cloud/storage/internal/invocation_id_generator.h"
#include "absl/strings/str_format.h"
#include <algorithm>
#include <array>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

// Initialize the random bit source with some minimal entropy.
InvocationIdGenerator::InvocationIdGenerator()
    : generator_(std::random_device{}()) {}

std::string InvocationIdGenerator::MakeInvocationId() {
  // A retry id is supposed to be a UUID V4 string. We assume you have read the
  // wikipedia page for the details:
  //     https://en.wikipedia.org/wiki/Universally_unique_identifier
  auto constexpr kVersionOctet = 6;
  auto constexpr kVariantOctet = 8;
  auto constexpr kVersion = static_cast<std::uint8_t>(4 << 4);
  auto constexpr kVersionMask = static_cast<std::uint8_t>(0b0000'1111);
  auto constexpr kVariant = static_cast<std::uint8_t>(1 << 5);
  auto constexpr kVariantMask = static_cast<std::uint8_t>(0b0001'1111);

  auto o = [this]() {
    auto constexpr kIdBitCount = 128;
    auto constexpr kArraySize = kIdBitCount / 8;
    std::array<std::uint8_t, kArraySize> buf;
    std::uniform_int_distribution<std::uint8_t> d(0, 255);

    std::lock_guard<std::mutex> lk(mu_);
    std::generate(buf.begin(), buf.end(), [&] { return d(generator_); });
    return buf;
  }();
  o[kVersionOctet] = (o[kVersionOctet] & kVersionMask) | kVersion;
  o[kVariantOctet] = (o[kVariantOctet] & kVariantMask) | kVariant;
  return absl::StrFormat(
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
      "%02x%02x%02x%02x%02x%02x",  //
      o[0], o[1], o[2], o[3],      //
      o[4], o[5],                  //
      o[6], o[7],                  //
      o[8], o[9],                  //
      o[10], o[11], o[12], o[13], o[14], o[15]);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

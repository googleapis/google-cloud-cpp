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

#include "google/cloud/internal/base64_transforms.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include <limits>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

constexpr char kPadding = '=';

// kCharToIndexExcessOne[] assumes an ASCII execution character set.
static_assert('A' == 65, "required by base64 decoder");

// A `unsigned char` must be able to hold *at least* the range [0, 255]. Meaning
// `std::string::value_type` can always hold an octet. If
// `std::numeric_limits<unsigned char>::max() > 255`, however, we have no way to
// base64 encode large values. So, we demand exactly 255.
// Should this assertion break on your platform, please file a bug at
//   https://github.com/googleapis/google-cloud-cpp/issues
static_assert(std::numeric_limits<unsigned char>::max() == 255,
              "required by base64 decoder");

// The extra braces are working around an old CLang bug that was fixed in 6.0
// https://bugs.llvm.org/show_bug.cgi?id=21629
constexpr std::array<char, 64> kIndexToChar = {{
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/',
}};

// The extra braces are working around an old CLang bug that was fixed in 6.0
// https://bugs.llvm.org/show_bug.cgi?id=21629
constexpr std::array<unsigned char, 256> kCharToIndexExcessOne = {{
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  63, 0,  0,  0,  64, 53, 54, 55, 56, 57, 58,
    59, 60, 61, 62, 0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7,
    8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
    26, 0,  0,  0,  0,  0,  0,  27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,
    38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
}};

/**
 * Decode up to 3 octets from 4 base64-encoded characters.
 *
 * The output octets characters are sent to @p sink, which must meet the
 * `std::is_invocable<Sink const&, unsigned char>` requirements.
 *
 * The function is inlined to avoid function call overhead in the case where @p
 * sink is a no-op.
 */
template <typename Sink>
inline bool Base64Fill(unsigned char p0, unsigned char p1, unsigned char p2,
                       unsigned char p3, Sink const& sink) {
  auto i0 = kCharToIndexExcessOne[p0];
  auto i1 = kCharToIndexExcessOne[p1];
  if (--i0 >= 64 || --i1 >= 64) return false;
  if (p3 == kPadding) {
    if (p2 == kPadding) {
      if ((i1 & 0xf) != 0) return false;
      sink(static_cast<unsigned char>(i0 << 2 | i1 >> 4));
      return true;
    }
    auto i2 = kCharToIndexExcessOne[p2];
    if (--i2 >= 64 || (i2 & 0x3) != 0) return false;
    sink(static_cast<unsigned char>(i0 << 2 | i1 >> 4));
    sink(static_cast<unsigned char>(i1 << 4 | i2 >> 2));
    return true;
  }
  auto i2 = kCharToIndexExcessOne[p2];
  auto i3 = kCharToIndexExcessOne[p3];
  if (--i2 >= 64 || --i3 >= 64) return false;
  sink(static_cast<unsigned char>(i0 << 2 | i1 >> 4));
  sink(static_cast<unsigned char>(i1 << 4 | i2 >> 2));
  sink(static_cast<unsigned char>(i2 << 6 | i3));
  return true;
}

Status Base64DecodingError(std::string const& input,
                           std::string::const_iterator p) {
  auto const offset = std::distance(input.begin(), p);
  auto const bad_chunk = input.substr(offset, 4);
  return Status(StatusCode::kInvalidArgument,
                absl::StrCat("Invalid base64 chunk \"", bad_chunk,
                             "\" at offset ", offset));
}

template <typename Sink>
Status Base64DecodeGeneric(std::string const& input, Sink const& sink) {
  auto p = input.begin();
  for (; std::distance(p, input.end()) >= 4; p = std::next(p, 4)) {
    if (!Base64Fill(*p, *(p + 1), *(p + 2), *(p + 3), sink)) break;
  }
  if (p != input.end()) return Base64DecodingError(input, p);
  return Status{};
}

}  // namespace

void Base64Encoder::Flush() {
  unsigned int const v = buf_[0] << 16 | buf_[1] << 8 | buf_[2];
  rep_.push_back(kIndexToChar[v >> 18]);
  rep_.push_back(kIndexToChar[v >> 12 & 0x3f]);
  rep_.push_back(kIndexToChar[v >> 6 & 0x3f]);
  rep_.push_back(kIndexToChar[v & 0x3f]);
  len_ = 0;
}

std::string Base64Encoder::FlushAndPad() && {
  switch (len_) {
    case 2: {
      unsigned int const v = buf_[0] << 16 | buf_[1] << 8;
      rep_.push_back(kIndexToChar[v >> 18]);
      rep_.push_back(kIndexToChar[v >> 12 & 0x3f]);
      rep_.push_back(kIndexToChar[v >> 6 & 0x3f]);
      rep_.push_back(kPadding);
      break;
    }
    case 1: {
      unsigned int const v = buf_[0] << 16;
      rep_.push_back(kIndexToChar[v >> 18]);
      rep_.push_back(kIndexToChar[v >> 12 & 0x3f]);
      rep_.append(2, kPadding);
      break;
    }
    case 0:
      break;
  }
  return std::move(rep_);
}

void Base64Decoder::Iterator::Fill() {
  if (pos_ != end_) {
    unsigned char p0 = *pos_++;
    unsigned char p1 = *pos_++;
    unsigned char p2 = *pos_++;
    unsigned char p3 = *pos_++;
    auto i0 = kCharToIndexExcessOne[p0] - 1;
    auto i1 = kCharToIndexExcessOne[p1] - 1;
    if (p3 == kPadding) {
      if (p2 == kPadding) {
        buf_[++len_] = static_cast<unsigned char>(i0 << 2 | i1 >> 4);
      } else {
        auto i2 = kCharToIndexExcessOne[p2] - 1;
        buf_[++len_] = static_cast<unsigned char>(i1 << 4 | i2 >> 2);
        buf_[++len_] = static_cast<unsigned char>(i0 << 2 | i1 >> 4);
      }
    } else {
      auto i2 = kCharToIndexExcessOne[p2] - 1;
      auto i3 = kCharToIndexExcessOne[p3] - 1;
      buf_[++len_] = static_cast<unsigned char>(i2 << 6 | i3);
      buf_[++len_] = static_cast<unsigned char>(i1 << 4 | i2 >> 2);
      buf_[++len_] = static_cast<unsigned char>(i0 << 2 | i1 >> 4);
    }
  }
}

Status ValidateBase64String(std::string const& input) {
  return Base64DecodeGeneric(input, [](unsigned char) {});
}

StatusOr<std::vector<std::uint8_t>> Base64DecodeToBytes(
    std::string const& input) {
  std::vector<std::uint8_t> result;
  auto status = Base64DecodeGeneric(
      input, [&result](unsigned char c) { result.push_back(c); });
  if (!status.ok()) return status;
  return result;
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

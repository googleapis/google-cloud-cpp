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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BASE64_TRANSFORMS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BASE64_TRANSFORMS_H

#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <array>
#include <cstddef>
#include <iterator>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

class Base64Encoder {
 public:
  explicit Base64Encoder() = default;
  void PushBack(unsigned char c) {
    buf_[len_++] = c;
    if (len_ == buf_.size()) Flush();
  }
  std::string FlushAndPad() &&;

 private:
  void Flush();

  std::string rep_;      // encoded
  std::size_t len_ = 0;  // buf_[0 .. len_-1] pending encode
  std::array<unsigned char, 3> buf_;
};

struct Base64Decoder {
  class Iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = unsigned char;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    Iterator(std::string::const_iterator begin, std::string::const_iterator end)
        : pos_(begin), end_(end), len_(0) {
      Fill();
    }

    void Fill();

    reference operator*() { return buf_[len_]; }
    pointer operator->() { return &buf_[len_]; }

    Iterator& operator++() {
      if (--len_ == 0) Fill();
      return *this;
    }
    Iterator operator++(int) {
      auto const old = *this;
      operator++();
      return old;
    }

    friend bool operator==(Iterator const& a, Iterator const& b) {
      return a.pos_ == b.pos_ && a.len_ == b.len_;
    }
    friend bool operator!=(Iterator const& a, Iterator const& b) {
      return !(a == b);
    }

   private:
    std::string::const_iterator pos_;  // [pos_ .. end_) pending decode
    std::string::const_iterator end_;
    std::size_t len_;  // buf_[len_ .. 1] decoded
    std::array<value_type, 1 + 3> buf_;
  };

  explicit Base64Decoder(std::string const& rep) : rep_(rep) {}
  Iterator begin() { return Iterator(rep_.begin(), rep_.end()); }
  Iterator end() { return Iterator(rep_.end(), rep_.end()); }

  std::string const& rep_;  // encoded
};

Status Base64DecodingError(std::string const& base64,
                           std::string::const_iterator p);

std::pair<std::array<unsigned char, 3>, std::size_t> Base64Fill(
    unsigned char p0, unsigned char p1, unsigned char p2, unsigned char p3);

template <typename Sink>
Status FromBase64(std::string const& base64, Sink const& sink) {
  auto p = base64.begin();
  for (; std::distance(p, base64.end()) >= 4; p = std::next(p, 4)) {
    auto r = Base64Fill(*p, *(p + 1), *(p + 2), *(p + 3));
    if (r.second == 0) return Base64DecodingError(base64, p);
    for (auto* o = r.first.begin(); o != r.first.begin() + r.second; ++o) {
      sink(*o);
    }
  }
  if (p != base64.end()) return Base64DecodingError(base64, p);
  return Status{};
}

Status ValidateBase64String(std::string const& input);

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BASE64_TRANSFORMS_H

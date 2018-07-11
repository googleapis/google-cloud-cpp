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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_STREAM_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_STREAM_H_

#include "google/cloud/storage/internal/raw_client.h"
#include <iostream>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * A `std::basic_streambuf` to read from a GCS Object.
 */
class ObjectReadStreamBuf : public std::basic_streambuf<char> {
 public:
  ObjectReadStreamBuf() : std::basic_streambuf<char>(), response_{} {
    RepositionInputSequence();
  }

  explicit ObjectReadStreamBuf(std::shared_ptr<internal::RawClient> client,
                               internal::ReadObjectRangeRequest request)
      : std::basic_streambuf<char>(),
        client_(std::move(client)),
        request_(std::move(request)),
        response_{} {
    RepositionInputSequence();
  }

  ObjectReadStreamBuf(ObjectReadStreamBuf&& rhs) noexcept
      : std::basic_streambuf<char>(),
        client_(std::move(rhs.client_)),
        request_(std::move(rhs.request_)),
        response_(std::move(rhs.response_)) {
    RepositionInputSequence();
  }

  ObjectReadStreamBuf& operator=(ObjectReadStreamBuf&& rhs) noexcept {
    client_ = std::move(rhs.client_);
    request_ = std::move(rhs.request_);
    response_ = std::move(rhs.response_);
    RepositionInputSequence();
    return *this;
  }

  ObjectReadStreamBuf(ObjectReadStreamBuf const&) = delete;
  ObjectReadStreamBuf& operator=(ObjectReadStreamBuf const&) = delete;
  ~ObjectReadStreamBuf() override = default;

  bool IsOpen() const { return static_cast<bool>(client_); }
  void Close() { client_.reset(); }

 protected:
  /// Stream: How many more characters?
  std::streamsize showmanyc() override;

  /// Read more data.
  int_type underflow() override;

 private:
  /// Update the iostream buffer based on buffer_.
  int_type RepositionInputSequence();

 private:
  std::shared_ptr<internal::RawClient> client_;
  internal::ReadObjectRangeRequest request_;
  internal::ReadObjectRangeResponse response_;
};

/**
 * A `std::basic_istream<char>` to read from a GCS Object.
 */
class ObjectReadStream : public std::basic_istream<char> {
 public:
  /**
   * Creates a stream not associated with any buffer.
   *
   * Attempts to use this stream will result in failures.
   */
  ObjectReadStream() : std::basic_istream<char>(&buf_), buf_() {}

  /**
   * Creates a stream associated with the give request.
   *
   * Reading from the stream will result in http requests to get more data
   * from the GCS object.
   *
   * @param client how to contact the GCS servers.
   * @param request an initialized request to read data. If no range is
   *     specified in this request then this reads the full object.
   */
  ObjectReadStream(std::shared_ptr<internal::RawClient> client,
                   internal::ReadObjectRangeRequest request)
      : std::basic_istream<char>(&buf_),
        buf_(std::move(client), std::move(request)) {}

  ObjectReadStream(ObjectReadStream const&) = delete;
  ObjectReadStream& operator=(ObjectReadStream const&) = delete;
  ObjectReadStream(ObjectReadStream&& rhs) noexcept
      : std::basic_istream<char>(&buf_), buf_(std::move(rhs.buf_)) {}
  ObjectReadStream& operator=(ObjectReadStream&& rhs) noexcept {
    buf_ = std::move(rhs.buf_);
    return *this;
  }
  ~ObjectReadStream() override = default;

  bool IsOpen() const { return buf_.IsOpen(); }
  void Close() { buf_.Close(); }

 private:
  ObjectReadStreamBuf buf_;
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_STREAM_H_

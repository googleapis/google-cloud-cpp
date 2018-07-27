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

#include "google/cloud/storage/internal/object_streambuf.h"
#include "google/cloud/storage/internal/raw_client.h"
#include <iostream>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
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
  ObjectReadStream() : std::basic_istream<char>(nullptr), buf_() {}

  /**
   * Creates a stream associated with the given `streambuf`.
   */
  explicit ObjectReadStream(std::unique_ptr<internal::ObjectReadStreambuf> buf)
      : std::basic_istream<char>(buf.get()), buf_(std::move(buf)) {}

  ObjectReadStream(ObjectReadStream&& rhs) noexcept
      : std::basic_istream<char>(rhs.buf_.get()), buf_(std::move(rhs.buf_)) {}

  ObjectReadStream& operator=(ObjectReadStream&& rhs) noexcept {
    buf_ = std::move(rhs.buf_);
    rdbuf(buf_.get());
    return *this;
  }

  ObjectReadStream(ObjectReadStream const&) = delete;
  ObjectReadStream& operator=(ObjectReadStream const&) = delete;

  /// Closes the stream (if necessary).
  ~ObjectReadStream() override;

  bool IsOpen() const { return buf_.get() != nullptr and buf_->IsOpen(); }
  internal::HttpResponse Close();

 private:
  std::unique_ptr<internal::ObjectReadStreambuf> buf_;
};

/**
 * A `std::basic_istream<char>` to read from a GCS Object.
 */
class ObjectWriteStream : public std::basic_ostream<char> {
 public:
  /**
   * Creates a stream not associated with any buffer.
   *
   * Attempts to use this stream will result in failures.
   */
  ObjectWriteStream() : std::basic_ostream<char>(nullptr), buf_() {}

  /**
   * Creates a stream associated with the give request.
   *
   * Reading from the stream will result in http requests to get more data
   * from the GCS object.
   *
   * @param buf an initialized ObjectWriteStreambuf to upload the data.
   */
  explicit ObjectWriteStream(
      std::unique_ptr<internal::ObjectWriteStreambuf> buf)
      : std::basic_ostream<char>(buf.get()), buf_(std::move(buf)) {}

  ObjectWriteStream(ObjectWriteStream&& rhs) noexcept
      : std::basic_ostream<char>(rhs.buf_.get()), buf_(std::move(rhs.buf_)) {}

  ObjectWriteStream& operator=(ObjectWriteStream&& rhs) noexcept {
    buf_ = std::move(rhs.buf_);
    rdbuf(buf_.get());
    return *this;
  }

  ObjectWriteStream(ObjectWriteStream const&) = delete;
  ObjectWriteStream& operator=(ObjectWriteStream const&) = delete;

  /// Closes the stream (if necessary).
  ~ObjectWriteStream() override;

  /// Return true while the stream is open.
  bool IsOpen() const { return buf_ != nullptr and buf_->IsOpen(); }

  /// Close the stream and return the metadata of the created object.
  ObjectMetadata Close();

  /// Close the stream and return the (unparsed) result, useful for testing.
  internal::HttpResponse CloseRaw();

 private:
  std::unique_ptr<internal::ObjectWriteStreambuf> buf_;
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_STREAM_H_

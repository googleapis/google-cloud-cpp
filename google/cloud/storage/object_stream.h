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
#include <ios>
#include <iostream>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Report checksum mismatches as exceptions.
 */
class HashMismatchError : public std::ios_base::failure {
 public:
  explicit HashMismatchError(std::string const& msg, std::string received,
                             std::string computed)
      : std::ios_base::failure(msg),
        received_hash_(std::move(received)),
        computed_hash_(std::move(computed)) {}

  std::string const& received_hash() const { return received_hash_; }
  std::string const& computed_hash() const { return computed_hash_; }

 private:
  std::string received_hash_;
  std::string computed_hash_;
};

/**
 * Defines a `std::basic_istream<char>` to read from a GCS Object.
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
      : std::basic_istream<char>(buf.get()), buf_(std::move(buf)) {
    // Prime the iostream machinery with a peek().  This will trigger a call to
    // underflow(), and will detect if the download failed. Without it, the
    // eof() bit is not initialized properly.
    peek();
  }

  ObjectReadStream(ObjectReadStream&& rhs) noexcept
      : ObjectReadStream(std::move(rhs.buf_)) {}

  ObjectReadStream& operator=(ObjectReadStream&& rhs) noexcept {
    buf_ = std::move(rhs.buf_);
    rdbuf(buf_.get());
    return *this;
  }

  ObjectReadStream(ObjectReadStream const&) = delete;
  ObjectReadStream& operator=(ObjectReadStream const&) = delete;

  /// Closes the stream (if necessary).
  ~ObjectReadStream() override;

  bool IsOpen() const { return (bool)buf_ && buf_->IsOpen(); }

  /**
   * Terminate the download, possibly before completing it.
   */
  void Close();

  //@{
  /**
   * Report any download errors.
   *
   * Note that errors may go undetected until the download completes.
   */
  Status const& status() const& { return buf_->status(); }

  /**
   * The received CRC32C checksum and the MD5 hash values as reported by GCS.
   *
   * When the download is finalized (via `Close()` or the end of file) the GCS
   * server reports the CRC32C checksum and, except for composite objects, the
   * MD5 hash of the data. This class compares the locally computed and received
   * hashes so applications can detect data download errors.
   *
   * The values are reported as comma separated `tag=value` pairs, e.g.
   * `crc32c=AAAAAA==,md5=1B2M2Y8AsgTpgAmY7PhCfg==`. The format of this string
   * is subject to change without notice, they are provided for informational
   * purposes only.
   *
   * @see https://cloud.google.com/storage/docs/hashes-etags for more
   *     information on checksums and hashes in GCS.
   */
  std::string const& received_hash() const { return buf_->received_hash(); }

  /**
   * The locally computed checksum and hashes, as a string.
   *
   * This object computes the CRC32C checksum and MD5 hash of the downloaded
   * data. Note that there are several cases where these values may be empty or
   * irrelevant, for example:
   *   - When reading only a portion of a blob the hash of that portion is
   *     irrelevant, note that GCS only reports the hashes for the full blob.
   *   - The application may disable the CRC32C and/or the MD5 hash computation.
   *
   * The string has the same format as the value returned by `received_hash()`.
   * Note that the format of this string is also subject to change without
   * notice.
   *
   * @see https://cloud.google.com/storage/docs/hashes-etags for more
   *     information on checksums and hashes in GCS.
   */
  std::string const& computed_hash() const { return buf_->computed_hash(); }

  /// The headers returned by the service, for debugging only.
  std::multimap<std::string, std::string> const& headers() const {
    return buf_->headers();
  }
  //@}

 private:
  std::unique_ptr<internal::ObjectReadStreambuf> buf_;
};

/**
 * Defines a `std::basic_ostream<char>` to write to a GCS Object.
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
  bool IsOpen() const { return buf_ != nullptr && buf_->IsOpen(); }

  /**
   * Close the stream, finalizing the upload.
   *
   * Closing a stream completes an upload and creates the uploaded object. On
   * failure it sets the `badbit` of the stream.
   *
   * The metadata of the uploaded object, or a detailed error status, is
   * accessible via the `metadata()` member function. Note that the metadata may
   * be empty if the application creates a stream with the `Fields("")`
   * parameter, applications cannot assume that all fields in the metadata are
   * filled on success.
   *
   * @throws If the application has enabled the exception mask this function may
   *     throw `std::ios_base::failure`.
   */
  void Close();

  //@{
  /**
   * Access the upload results.
   *
   * Note that calling these member functions before `Close()` is undefined
   * behavior.
   */
  StatusOr<ObjectMetadata> const& metadata() const& { return metadata_; }
  StatusOr<ObjectMetadata>&& metadata() && { return std::move(metadata_); }

  /**
   * The received CRC32C checksum and the MD5 hash values as reported by GCS.
   *
   * When the upload is finalized (via `Close()`) the GCS server reports the
   * CRC32C checksum and, if the object is not a composite object, the MDF hash
   * of the uploaded data. This class compares the reported hashes against
   * locally computed hash values, and reports an error if they do not match.
   *
   * The values are reported as comma separated `tag=value` pairs, e.g.
   * `crc32c=AAAAAA==,md5=1B2M2Y8AsgTpgAmY7PhCfg==`. The format of this string
   * is subject to change without notice, they are provided for informational
   * purposes only.
   *
   * @see https://cloud.google.com/storage/docs/hashes-etags for more
   *     information on checksums and hashes in GCS.
   */
  std::string const& received_hash() const { return buf_->received_hash(); }

  /**
   * The locally computed checksum and hashes, as a string.
   *
   * This object computes the CRC32C checksum and MD5 hash of the uploaded data.
   * There are several cases where these values may be empty or irrelevant, for
   * example:
   *   - When performing resumable uploads the stream may not have had access to
   *     the full data.
   *   - The application may disable the CRC32C and/or the MD5 hash computation.
   *
   * The string has the same format as the value returned by `received_hash()`.
   * Note that the format of this string is also subject to change without
   * notice.
   *
   * @see https://cloud.google.com/storage/docs/hashes-etags for more
   *     information on checksums and hashes in GCS.
   */
  std::string const& computed_hash() const { return buf_->computed_hash(); }

  /// The headers returned by the service, for debugging only.
  std::multimap<std::string, std::string> const& headers() const {
    return headers_;
  }

  /// The returned payload as a raw string, for debugging only.
  std::string const& payload() const { return payload_; }
  //@}

  /**
   * Returns the resumable upload session id for this upload.
   *
   * Note that this is an empty string for uploads that do not use resumable
   * upload session ids. `Client::WriteObject()` enables resumable uploads based
   * on the options set by the application.
   *
   * Furthermore, this value might change during an upload.
   */
  std::string const& resumable_session_id() const {
    return buf_->resumable_session_id();
  }

  /**
   * Returns the next expected byte.
   *
   * For non-resumable uploads this is always zero. Applications that use
   * resumable uploads can use this value to resend any data not committed in
   * the GCS.
   */
  std::uint64_t next_expected_byte() const {
    return buf_->next_expected_byte();
  }

  /**
   * Suspends an upload.
   *
   * This is a destructive operation. Using this object after calling this
   * function results in undefined behavior. Applications should copy any
   * necessary state (such as the value `resumable_session_id()`) before calling
   * this function.
   */
  void Suspend() &&;

 private:
  std::unique_ptr<internal::ObjectWriteStreambuf> buf_;
  StatusOr<ObjectMetadata> metadata_;
  std::multimap<std::string, std::string> headers_;
  std::string payload_;
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_STREAM_H_

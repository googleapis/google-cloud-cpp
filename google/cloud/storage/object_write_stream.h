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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_WRITE_STREAM_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_WRITE_STREAM_H

#include "google/cloud/storage/internal/object_write_streambuf.h"
#include "google/cloud/storage/version.h"
#include <map>
#include <memory>
#include <ostream>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Represents the headers returned in a streaming upload or download operation.
using HeadersMap = std::multimap<std::string, std::string>;

/**
 * Defines a `std::basic_ostream<char>` to write to a GCS object.
 *
 * This class is used to upload objects to GCS. It can handle objects of any
 * size, but keep the following considerations in mind:
 *
 * * This API is designed for applications that need to stream the object
 *   payload. If you have the payload as one large buffer consider using
 *   `storage::Client::InsertObject()`, it is simpler and faster in most cases.
 * * This API can be used to perform unformatted I/O, as well as formatted I/O
 *   using the familiar `operator<<` APIs. Note that formatted I/O typically
 *   implies some form of buffering and data copying. For best performance,
 *   consider using the [.write()][cpp-reference-write] member function.
 * * GCS expects to receive data in multiples of the *upload quantum* (256KiB).
 *   Sending a buffer that is not a multiple of this quantum terminates the
 *   upload. This constraints the implementation of buffered and unbuffered I/O
 *   as described below.
 *
 * @par Unformatted I/O
 * On a `.write()` call this class attempts to send the data immediately, this
 * this the unbuffered API after all. If any previously buffered data and the
 * data provided in the `.write()` call are larger than an upload quantum the
 * class sends data immediately. Any data in excess of a multiple of the upload
 * quantum are buffered for the next upload.
 *
 * These examples may clarify how this works:
 *   -# Consider a fresh `ObjectWriteStream` that receives a `.write()` call
 *      with 257 KiB of data. The first 256 KiB are immediately sent and the
 *      remaining 1 KiB is buffered for a future upload.
 *   -# If the same stream receives another `.write()` call with 256 KiB then it
 *      will send the buffered 1 KiB of data and the first 255 KiB from the new
 *      buffer. The last 1 KiB is buffered for a future upload.
 *   -# Consider a fresh `ObjectWriteStream` that receives a `.write()` call
 *      with 4 MiB of data. This data is sent immediately, and no data is
 *      buffered.
 *   -# Consider a stream with a 256 KiB buffer from previous buffered I/O (see
 *      below to understand how this might happen). If this stream receives a
 *      `.write()` call with 1024 KiB then both the 256 KiB and the 1024 KiB of
 *      data are uploaded immediately.
 *
 * @par Formatted I/O
 * When performing formatted I/O, typically used via `operator<<`, this class
 * will buffer data based on the`ClientOptions::upload_buffer_size()` setting.
 * Note that this setting is expressed in bytes, but it is always rounded (up)
 * to an upload quantum.
 *
 * @par Recommendations
 * For best performance uploading data we recommend using *exclusively* the
 * unbuffered I/O API. Furthermore, we recommend that applications use data in
 * multiples of the upload quantum in all calls to `.write()`. Larger buffers
 * result in better performance. Note that our
 * [empirical results][github-issue-2657] show that these improvements tapper
 * off around 32MiB or so.
 *
 * @par Suspending Uploads
 * Note that, as it is customary in C++, the destructor of this class finalizes
 * the upload. If you want to prevent the class from finalizing an upload, use
 * the `Suspend()` function.
 *
 * @par Example: starting a resumable upload.
 * @snippet storage_object_resumable_write_samples.cc start resumable upload
 *
 * @par Example: resuming a resumable upload.
 * @snippet storage_object_resumable_write_samples.cc resume resumable upload
 *
 * [cpp-reference-put]: https://en.cppreference.com/w/cpp/io/basic_ostream/put
 *
 * [cpp-reference-write]:
 * https://en.cppreference.com/w/cpp/io/basic_ostream/write
 *
 * [github-issue-2657]:
 * https://github.com/googleapis/google-cloud-cpp/issues/2657
 */
class ObjectWriteStream : public std::basic_ostream<char> {
 public:
  /**
   * Creates a stream not associated with any buffer.
   *
   * Attempts to use this stream will result in failures.
   */
  ObjectWriteStream();

  /**
   * Creates a stream associated with the give request.
   *
   * Reading from the stream will result in http requests to get more data
   * from the GCS object.
   *
   * @param buf an initialized ObjectWriteStreambuf to upload the data.
   */
  explicit ObjectWriteStream(
      std::unique_ptr<internal::ObjectWriteStreambuf> buf);

  ObjectWriteStream(ObjectWriteStream&& rhs) noexcept;

  ObjectWriteStream& operator=(ObjectWriteStream&& rhs) noexcept {
    ObjectWriteStream tmp(std::move(rhs));
    swap(tmp);
    return *this;
  }

  void swap(ObjectWriteStream& rhs) {
    basic_ostream<char>::swap(rhs);
    std::swap(buf_, rhs.buf_);
    rhs.set_rdbuf(rhs.buf_.get());
    set_rdbuf(buf_.get());
    std::swap(metadata_, rhs.metadata_);
    std::swap(headers_, rhs.headers_);
    std::swap(payload_, rhs.payload_);
  }

  ObjectWriteStream(ObjectWriteStream const&) = delete;
  ObjectWriteStream& operator=(ObjectWriteStream const&) = delete;

  /// Closes the stream (if necessary).
  ~ObjectWriteStream() override;

  /**
   * Return true if the stream is open to write more data.
   *
   * @note
   * write streams can be "born closed" when created using a previously
   * finalized upload session. Applications that restore a previous session
   * should check the state, for example:
   *
   * @code
   * auto stream = client.WriteObject(...,
   *     gcs::RestoreResumableUploadSession(session_id));
   * if (!stream.IsOpen() && stream.metadata().ok()) {
   *   std::cout << "Yay! The upload was finalized previously.\n";
   *   return;
   * }
   * @endcode
   */
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

  /**
   * The headers (if any) returned by the service. For debugging only.
   *
   * @warning The contents of these headers may change without notice. Unless
   *     documented in the API, headers may be removed or added by the service.
   *     Also note that the client library uses both the XML and JSON API,
   *     choosing between them based on the feature set (some functionality is
   *     only available through the JSON API), and performance.  Consequently,
   *     the headers may be different on requests using different features.
   *     Likewise, the headers may change from one version of the library to the
   *     next, as we find more (or different) opportunities for optimization.
   */
  HeadersMap const& headers() const { return headers_; }

  /// The returned payload as a raw string, for debugging only.
  std::string const& payload() const { return payload_; }
  //@}

  /**
   * Returns the resumable upload session id for this upload.
   *
   * Note that this is an empty string for uploads that do not use resumable
   * upload session ids. `Client::WriteObject()` enables resumable uploads based
   * on the options set by the application.
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
   *
   * @snippet storage_object_resumable_write_samples.cc suspend resumable upload
   */
  void Suspend() &&;

  /**
   * Returns the status of partial errors.
   *
   * Application may write multiple times before closing the stream, this
   * function gives the capability to find out status even before stream
   * closure.
   *
   * This function is different than `metadata()` as calling `metadata()`
   * before Close() is undefined.
   */
  Status last_status() const { return buf_->last_status(); }

 private:
  /**
   * Closes the underlying object write stream.
   */
  void CloseBuf();

  std::unique_ptr<internal::ObjectWriteStreambuf> buf_;
  StatusOr<ObjectMetadata> metadata_;
  std::multimap<std::string, std::string> headers_;
  std::string payload_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_WRITE_STREAM_H

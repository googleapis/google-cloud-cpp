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

#include "google/cloud/storage/internal/object_streambuf.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/object_stream.h"
#include "google/cloud/log.h"
#include <cstring>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
ObjectReadStreambuf::ObjectReadStreambuf(
    ReadObjectRangeRequest const& request,
    std::unique_ptr<ObjectReadSource> source, std::streamoff pos_in_stream)
    : source_(std::move(source)), source_pos_(pos_in_stream) {
  hash_validator_ = CreateHashValidator(request);
}

ObjectReadStreambuf::ObjectReadStreambuf(ReadObjectRangeRequest const& request,
                                         Status status)
    : source_(new ObjectReadErrorSource(status)), source_pos_(-1) {
  // TODO(coryan) - revisit this, we probably do not need the validator.
  hash_validator_ = CreateHashValidator(request);
  status_ = std::move(status);
}

bool ObjectReadStreambuf::IsOpen() const { return source_->IsOpen(); }

void ObjectReadStreambuf::Close() {
  auto response = source_->Close();
  if (!response.ok()) {
    ReportError(std::move(response).status());
  }
}

StatusOr<ObjectReadStreambuf::int_type> ObjectReadStreambuf::Peek() {
  if (!IsOpen()) {
    // The stream is closed, reading from a closed stream can happen if there is
    // no object to read from, or the object is empty. In that case just setup
    // an empty (but valid) region and verify the checksums.
    SetEmptyRegion();
    return traits_type::eof();
  }

  auto constexpr kInitialPeekRead = 128 * 1024;
  current_ios_buffer_.resize(kInitialPeekRead);
  std::size_t n = current_ios_buffer_.size();
  StatusOr<ReadSourceResult> read_result =
      source_->Read(current_ios_buffer_.data(), n);
  if (!read_result.ok()) {
    return std::move(read_result).status();
  }
  source_pos_ += read_result->bytes_received;
  // assert(n <= current_ios_buffer_.size())
  current_ios_buffer_.resize(read_result->bytes_received);

  for (auto const& kv : read_result->response.headers) {
    hash_validator_->ProcessHeader(kv.first, kv.second);
    headers_.emplace(kv.first, kv.second);
  }
  if (read_result->response.status_code >= HttpStatusCode::kMinNotSuccess) {
    return AsStatus(read_result->response);
  }

  if (!current_ios_buffer_.empty()) {
    char* data = current_ios_buffer_.data();
    hash_validator_->Update(data, current_ios_buffer_.size());
    setg(data, data, data + current_ios_buffer_.size());
    return traits_type::to_int_type(*data);
  }

  // This is an actual EOF, there is no more data to download, create an
  // empty (but valid) region:
  SetEmptyRegion();
  return traits_type::eof();
}

ObjectReadStreambuf::int_type ObjectReadStreambuf::underflow() {
  auto next_char = Peek();
  if (!next_char) {
    return ReportError(next_char.status());
  }

  if (*next_char == traits_type::eof()) {
    hash_validator_result_ = std::move(*hash_validator_).Finish();
    if (hash_validator_result_.is_mismatch) {
      std::string msg;
      msg += __func__;
      msg += "(): mismatched hashes in download";
      msg += " computed=";
      msg += hash_validator_result_.computed;
      msg += " received=";
      msg += hash_validator_result_.received;
      if (status_.ok()) {
        // If there is an existing error, we should report that instead because
        // it is more specific, for example, every permanent network error will
        // produce invalid checksums, but that is not the interesting
        // information.
        status_ = Status(StatusCode::kDataLoss, msg);
      }
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
      throw HashMismatchError(msg, hash_validator_result_.received,
                              hash_validator_result_.computed);
#else
      return traits_type::eof();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    }
  }

  return *next_char;
}

std::streamsize ObjectReadStreambuf::xsgetn(char* s, std::streamsize count) {
  GCP_LOG(INFO) << __func__ << "(): count=" << count
                << ", in_avail=" << in_avail() << ", status=" << status_;
  // This function optimizes stream.read(), the data is copied directly from the
  // data source (typically libcurl) into a buffer provided by the application.
  std::streamsize offset = 0;
  if (!status_.ok()) {
    return 0;
  }

  auto const* function_name = __func__;
  auto run_validator_if_closed = [this, function_name, &offset](Status s) {
    ReportError(std::move(s));
    // Only validate the checksums once the stream is closed.
    if (IsOpen()) {
      return offset;
    }
    hash_validator_result_ = std::move(*hash_validator_).Finish();
    if (!hash_validator_result_.is_mismatch) {
      return offset;
    }
    std::string msg;
    msg += function_name;
    msg += "(): mismatched hashes in download";
    msg += ", computed=";
    msg += hash_validator_result_.computed;
    msg += ", received=";
    msg += hash_validator_result_.received;
    if (status_.ok()) {
      // If there is an existing error, we should report that instead because
      // it is more specific, for example, every permanent network error will
      // produce invalid checksums, but that is not the interesting information.
      status_ = Status(StatusCode::kDataLoss, msg);
    }
    // The only way to report errors from a std::basic_streambuf<> (which this
    // class derives from) is to throw exceptions:
    //   https://stackoverflow.com/questions/50716688/how-to-set-the-badbit-of-a-stream-by-a-customized-streambuf
    // but we need to be able to report errors when the application has disabled
    // exceptions via `-fno-exceptions` or a similar option. In that case we set
    // `status_`, and report the error as an 0-byte read. This is obviously not
    // ideal, but it is the best we can do when the application disables the
    // standard mechanism to signal errors.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    throw HashMismatchError(msg, hash_validator_result_.received,
                            hash_validator_result_.computed);
#else
    return std::streamsize(0);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  };

  // Maybe the internal get area is enough to satisfy this request, no need to
  // read more in that case:
  auto from_internal = (std::min)(count, in_avail());
  if (from_internal > 0) {
    std::memcpy(s, gptr(), static_cast<std::size_t>(from_internal));
  }
  gbump(static_cast<int>(from_internal));
  offset += from_internal;
  if (offset >= count) {
    GCP_LOG(INFO) << __func__ << "(): count=" << count
                  << ", in_avail=" << in_avail() << ", offset=" << offset;
    return run_validator_if_closed(Status());
  }

  StatusOr<ReadSourceResult> read_result =
      source_->Read(s + offset, static_cast<std::size_t>(count - offset));
  // If there was an error set the internal state, but we still return the
  // number of bytes.
  if (!read_result) {
    GCP_LOG(INFO) << __func__ << "(): count=" << count
                  << ", in_avail=" << in_avail() << ", offset=" << offset
                  << ", status=" << read_result.status();
    return run_validator_if_closed(std::move(read_result).status());
  }
  GCP_LOG(INFO) << __func__ << "(): count=" << count
                << ", in_avail=" << in_avail() << ", offset=" << offset
                << ", read_result->bytes_received="
                << read_result->bytes_received;

  hash_validator_->Update(s + offset, read_result->bytes_received);
  offset += read_result->bytes_received;
  source_pos_ += read_result->bytes_received;

  for (auto const& kv : read_result->response.headers) {
    hash_validator_->ProcessHeader(kv.first, kv.second);
    headers_.emplace(kv.first, kv.second);
  }
  if (read_result->response.status_code >= HttpStatusCode::kMinNotSuccess) {
    return run_validator_if_closed(AsStatus(read_result->response));
  }
  return run_validator_if_closed(Status());
}

ObjectReadStreambuf::int_type ObjectReadStreambuf::ReportError(Status status) {
  // The only way to report errors from a std::basic_streambuf<> (which this
  // class derives from) is to throw exceptions:
  //   https://stackoverflow.com/questions/50716688/how-to-set-the-badbit-of-a-stream-by-a-customized-streambuf
  // but we need to be able to report errors when the application has disabled
  // exceptions via `-fno-exceptions` or a similar option. In that case we set
  // `status_`, and report the error as an EOF. This is obviously not ideal,
  // but it is the best we can do when the application disables the standard
  // mechanism to signal errors.
  if (status.ok()) {
    return traits_type::eof();
  }
  status_ = std::move(status);
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  google::cloud::internal::ThrowStatus(status_);
#else
  return traits_type::eof();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

void ObjectReadStreambuf::SetEmptyRegion() {
  current_ios_buffer_.clear();
  current_ios_buffer_.push_back('\0');
  char* data = &current_ios_buffer_[0];
  setg(data, data + 1, data + 1);
}

ObjectWriteStreambuf::ObjectWriteStreambuf(
    std::unique_ptr<ResumableUploadSession> upload_session,
    std::size_t max_buffer_size, std::unique_ptr<HashValidator> hash_validator)
    : upload_session_(std::move(upload_session)),
      max_buffer_size_(UploadChunkRequest::RoundUpToQuantum(max_buffer_size)),
      hash_validator_(std::move(hash_validator)),
      last_response_(ResumableUploadResponse{
          {}, 0, {}, ResumableUploadResponse::kInProgress, {}}) {
  current_ios_buffer_.resize(max_buffer_size_);
  auto pbeg = current_ios_buffer_.data();
  auto pend = pbeg + current_ios_buffer_.size();
  setp(pbeg, pend);
  // Sessions start in a closed state for uploads that have already been
  // finalized.
  if (upload_session_->done()) {
    last_response_ = upload_session_->last_response();
  }
}

ObjectReadStreambuf::pos_type ObjectReadStreambuf::seekpos(
    pos_type /*pos*/, std::ios_base::openmode /*which*/) {
  // TODO(4013): Implement proper seeking.
  return -1;
}

ObjectReadStreambuf::pos_type ObjectReadStreambuf::seekoff(
    off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) {
  // TODO(4013): Implement proper seeking.
  // Seeking is non-trivial because the hash validator and `source_` have to be
  // recreated in the general case, which doesn't fit the current code
  // organization.  We can, however, at least implement the bare minimum of this
  // function allowing `tellg()` to work.
  if (which == std::ios_base::in && dir == std::ios_base::cur && off == 0) {
    return source_pos_ - in_avail();
  }
  return -1;
}

StatusOr<ResumableUploadResponse> ObjectWriteStreambuf::Close() {
  pubsync();
  GCP_LOG(INFO) << __func__ << "()";
  return FlushFinal();
}

bool ObjectWriteStreambuf::IsOpen() const {
  return static_cast<bool>(upload_session_) && !upload_session_->done();
}

bool ObjectWriteStreambuf::ValidateHash(ObjectMetadata const& meta) {
  hash_validator_->ProcessMetadata(meta);
  hash_validator_result_ = std::move(*hash_validator_).Finish();
  return !hash_validator_result_.is_mismatch;
}

int ObjectWriteStreambuf::sync() {
  auto result = Flush();
  if (!result.ok()) {
    return traits_type::eof();
  }
  return 0;
}

std::streamsize ObjectWriteStreambuf::xsputn(char const* s,
                                             std::streamsize count) {
  if (!IsOpen()) {
    return traits_type::eof();
  }

  std::streamsize bytes_copied = 0;
  while (bytes_copied != count) {
    std::streamsize remaining_buffer_size = epptr() - pptr();
    std::streamsize bytes_to_copy =
        std::min(count - bytes_copied, remaining_buffer_size);
    std::copy(s, s + bytes_to_copy, pptr());
    pbump(static_cast<int>(bytes_to_copy));
    bytes_copied += bytes_to_copy;
    s += bytes_to_copy;
    last_response_ = Flush();
    // Upload failures are irrecoverable because the internal buffer is opaque
    // to the caller, so there is no way to know what byte range to specify
    // next.  Replace it with a SessionError so next_expected_byte and
    // resumable_session_id can still be retrieved.
    if (!last_response_.ok()) {
      upload_session_ = std::unique_ptr<ResumableUploadSession>(
          new ResumableUploadSessionError(last_response_.status(),
                                          next_expected_byte(),
                                          resumable_session_id()));
      return traits_type::eof();
    }
  }
  return count;
}

ObjectWriteStreambuf::int_type ObjectWriteStreambuf::overflow(int_type ch) {
  if (!IsOpen()) {
    return traits_type::eof();
  }
  if (traits_type::eq_int_type(ch, traits_type::eof())) {
    // For ch == EOF this function must do nothing and return any value != EOF.
    return 0;
  }
  // If the buffer is full flush it immediately.
  auto result = Flush();
  if (!result.ok()) {
    return traits_type::eof();
  }
  // Make sure there is now room in the buffer for the char.
  if (pptr() == epptr()) {
    return traits_type::eof();
  }
  // Push the character into the current buffer.
  *pptr() = traits_type::to_char_type(ch);
  pbump(1);
  return ch;
}

StatusOr<ResumableUploadResponse> ObjectWriteStreambuf::FlushFinal() {
  if (!IsOpen()) {
    return last_response_;
  }
  // Shorten the buffer to the actual used size.
  auto actual_size = static_cast<std::size_t>(pptr() - pbase());
  std::size_t upload_size = upload_session_->next_expected_byte() + actual_size;
  hash_validator_->Update(pbase(), actual_size);

  std::string to_upload(pbase(), actual_size);
  last_response_ = upload_session_->UploadFinalChunk(to_upload, upload_size);
  if (!last_response_) {
    // This was an unrecoverable error, time to store status and signal an
    // error.
    return last_response_;
  }
  // Reset the iostream put area with valid pointers, but empty.
  current_ios_buffer_.resize(1);
  auto pbeg = current_ios_buffer_.data();
  setp(pbeg, pbeg);

  upload_session_.reset();

  return last_response_;
}

StatusOr<ResumableUploadResponse> ObjectWriteStreambuf::Flush() {
  if (!IsOpen()) {
    return last_response_;
  }

  auto actual_size = static_cast<std::size_t>(pptr() - pbase());
  if (actual_size < max_buffer_size_) {
    return last_response_;
  }

  auto chunk_count = actual_size / UploadChunkRequest::kChunkSizeQuantum;
  auto chunk_size = chunk_count * UploadChunkRequest::kChunkSizeQuantum;
  // GCS upload returns an updated range header that sets the next expected
  // byte. Check to make sure it remains consistent with the bytes stored in the
  // buffer.
  auto expected_next_byte = upload_session_->next_expected_byte() + chunk_size;

  hash_validator_->Update(pbase(), chunk_size);
  StatusOr<ResumableUploadResponse> result;
  std::string to_send(pbase(), chunk_size);
  last_response_ = upload_session_->UploadChunk(to_send);
  if (!last_response_) {
    return last_response_;
  }
  auto actual_next_byte = upload_session_->next_expected_byte();
  auto bytes_uploaded = static_cast<int64_t>(chunk_size);
  if (actual_next_byte < expected_next_byte) {
    bytes_uploaded -= expected_next_byte - actual_next_byte;
    if (bytes_uploaded < 0) {
      std::ostringstream error_message;
      error_message << "Could not continue upload stream. GCS requested byte "
                    << actual_next_byte << " which has already been uploaded.";
      return Status(StatusCode::kAborted, error_message.str());
    }
  } else if (actual_next_byte > expected_next_byte) {
    std::ostringstream error_message;
    error_message << "Could not continue upload stream. "
                  << "GCS requested unexpected byte. (expected: "
                  << expected_next_byte << ", actual: " << actual_next_byte
                  << ")";
    return Status(StatusCode::kAborted, error_message.str());
  }
  std::copy(pbase() + bytes_uploaded, epptr(), pbase());
  setp(pbase(), epptr());
  pbump(static_cast<int>(actual_size - bytes_uploaded));
  return last_response_;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

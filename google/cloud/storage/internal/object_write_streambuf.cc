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

#include "google/cloud/storage/internal/object_write_streambuf.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/version.h"
#include "absl/memory/memory.h"
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

ObjectWriteStreambuf::ObjectWriteStreambuf(
    std::unique_ptr<ResumableUploadSession> upload_session,
    std::size_t max_buffer_size, std::unique_ptr<HashFunction> hash_function,
    HashValues known_hashes, std::unique_ptr<HashValidator> hash_validator,
    AutoFinalizeConfig auto_finalize)
    : upload_session_(std::move(upload_session)),
      max_buffer_size_(UploadChunkRequest::RoundUpToQuantum(max_buffer_size)),
      hash_function_(std::move(hash_function)),
      known_hashes_(std::move(known_hashes)),
      hash_validator_(std::move(hash_validator)),
      auto_finalize_(auto_finalize),
      last_response_(ResumableUploadResponse{
          {}, 0, {}, ResumableUploadResponse::kInProgress, {}}) {
  current_ios_buffer_.resize(max_buffer_size_);
  auto* pbeg = current_ios_buffer_.data();
  auto* pend = pbeg + current_ios_buffer_.size();
  setp(pbeg, pend);
  // Sessions start in a closed state for uploads that have already been
  // finalized.
  if (upload_session_->done()) {
    last_response_ = upload_session_->last_response();
  }
}

void ObjectWriteStreambuf::AutoFlushFinal() {
  if (auto_finalize_ != AutoFinalizeConfig::kEnabled) return;
  Close();
}

StatusOr<ResumableUploadResponse> ObjectWriteStreambuf::Close() {
  FlushFinal();
  return last_response_;
}

bool ObjectWriteStreambuf::IsOpen() const {
  return static_cast<bool>(upload_session_) && !upload_session_->done();
}

bool ObjectWriteStreambuf::ValidateHash(ObjectMetadata const& meta) {
  // This function is called once the stream is "closed", via an explicit
  // `Close()` call, or a permanent error, or (more rarely) implicitly because
  // the application is using the X-Upload-Content-Length header. In any case,
  // once closed the stream will never use `hash_validator_` or `hash_function_`
  // again, as the pre-conditions for `Flush*()` prevent this.
  //
  // If the application has set X-Upload-Content-Length then the stream may be
  // implicitly closed. In that case we need to compute the hashes.
  if (hash_function_) {
    auto function = std::move(hash_function_);
    hash_values_ = std::move(*function).Finish();
  }
  auto validator = std::move(hash_validator_);
  validator->ProcessMetadata(meta);
  hash_validator_result_ = std::move(*validator).Finish(hash_values_);
  computed_hash_ = FormatComputedHashes(hash_validator_result_);
  received_hash_ = FormatReceivedHashes(hash_validator_result_);
  return !hash_validator_result_.is_mismatch;
}

int ObjectWriteStreambuf::sync() {
  Flush();
  return !last_response_ ? traits_type::eof() : 0;
}

std::streamsize ObjectWriteStreambuf::xsputn(char const* s,
                                             std::streamsize count) {
  if (!IsOpen()) return traits_type::eof();

  auto const actual_size = put_area_size();
  if (count + actual_size >= max_buffer_size_) {
    if (actual_size == 0) {
      FlushRoundChunk({ConstBuffer(s, static_cast<std::size_t>(count))});
    } else {
      FlushRoundChunk({
          ConstBuffer(pbase(), actual_size),
          ConstBuffer(s, static_cast<std::size_t>(count)),
      });
    }
    if (!last_response_) return traits_type::eof();
  } else {
    std::copy(s, s + count, pptr());
    pbump(static_cast<int>(count));
  }
  return count;
}

ObjectWriteStreambuf::int_type ObjectWriteStreambuf::overflow(int_type ch) {
  // For ch == EOF this function must do nothing and return any value != EOF.
  if (traits_type::eq_int_type(ch, traits_type::eof())) return 0;
  if (!IsOpen()) return traits_type::eof();

  auto actual_size = put_area_size();
  if (actual_size >= max_buffer_size_) Flush();
  *pptr() = traits_type::to_char_type(ch);
  pbump(1);
  return last_response_ ? ch : traits_type::eof();
}

void ObjectWriteStreambuf::FlushFinal() {
  if (!IsOpen()) return;

  // Calculate the portion of the buffer that needs to be uploaded, if any.
  auto const actual_size = put_area_size();
  auto const upload_size = upload_session_->next_expected_byte() + actual_size;
  hash_function_->Update(pbase(), actual_size);

  // After this point the session will be closed, and no more calls to the hash
  // function are possible.
  auto function = std::move(hash_function_);
  hash_values_ = std::move(*function).Finish();
  last_response_ = upload_session_->UploadFinalChunk(
      {ConstBuffer(pbase(), actual_size)}, upload_size,
      Merge(known_hashes_, hash_values_));

  // Reset the iostream put area with valid pointers, but empty.
  current_ios_buffer_.resize(1);
  auto* pbeg = current_ios_buffer_.data();
  setp(pbeg, pbeg);

  // Close the stream
  upload_session_.reset();
}

void ObjectWriteStreambuf::Flush() {
  if (!IsOpen()) return;

  auto actual_size = put_area_size();
  if (actual_size < UploadChunkRequest::kChunkSizeQuantum) return;

  ConstBufferSequence payload{ConstBuffer(pbase(), actual_size)};
  FlushRoundChunk(payload);
}

void ObjectWriteStreambuf::FlushRoundChunk(ConstBufferSequence buffers) {
  auto actual_size = TotalBytes(buffers);
  auto chunk_count = actual_size / UploadChunkRequest::kChunkSizeQuantum;
  auto rounded_size = chunk_count * UploadChunkRequest::kChunkSizeQuantum;

  // Trim the buffers to the rounded chunk we will actually upload.
  auto payload = buffers;
  while (actual_size > rounded_size && !payload.empty()) {
    auto const n =
        (std::min)(actual_size - rounded_size, payload.back().size());
    payload.back().remove_suffix(n);
    actual_size -= n;
    if (payload.back().empty()) payload.pop_back();
  }

  for (auto const& b : payload) {
    hash_function_->Update(b.data(), b.size());
  }

  // GCS upload returns an updated range header that sets the next expected
  // byte. Check to make sure it remains consistent with the bytes stored in the
  // buffer.
  auto first_buffered_byte = upload_session_->next_expected_byte();
  auto expected_next_byte = upload_session_->next_expected_byte() + actual_size;
  last_response_ = upload_session_->UploadChunk(payload);

  if (last_response_) {
    // Reset the internal buffer and copy any trailing bytes from `buffers` to
    // it.
    auto* pbeg = current_ios_buffer_.data();
    setp(pbeg, pbeg + current_ios_buffer_.size());
    PopFrontBytes(buffers, rounded_size);
    for (auto const& b : buffers) {
      std::copy(b.begin(), b.end(), pptr());
      pbump(static_cast<int>(b.size()));
    }

    // We cannot use the last committed byte in `last_response_` because when
    // using X-Upload-Content-Length GCS returns 0 when the upload completed
    // even if no "final chunk" is sent.  The resumable upload classes know how
    // to deal with this mess, so let's not duplicate that code here.
    auto actual_next_byte = upload_session_->next_expected_byte();
    if (actual_next_byte < expected_next_byte &&
        actual_next_byte < first_buffered_byte) {
      std::ostringstream error_message;
      error_message << "Could not continue upload stream. GCS requested byte "
                    << actual_next_byte << " which has already been uploaded.";
      last_response_ = Status(StatusCode::kAborted, error_message.str());
    } else if (actual_next_byte > expected_next_byte) {
      std::ostringstream error_message;
      error_message << "Could not continue upload stream. "
                    << "GCS requested unexpected byte. (expected: "
                    << expected_next_byte << ", actual: " << actual_next_byte
                    << ")";
      last_response_ = Status(StatusCode::kAborted, error_message.str());
    }
  }

  // Upload failures are irrecoverable because the internal buffer is opaque
  // to the caller, so there is no way to know what byte range to specify
  // next.  Replace it with a SessionError so next_expected_byte and
  // resumable_session_id can still be retrieved.
  if (!last_response_) {
    upload_session_ = absl::make_unique<ResumableUploadSessionError>(
        last_response_.status(), next_expected_byte(), resumable_session_id());
  }
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

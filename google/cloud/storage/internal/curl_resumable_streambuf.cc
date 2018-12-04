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

#include "google/cloud/storage/internal/curl_resumable_streambuf.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

CurlResumableStreambuf::CurlResumableStreambuf(
    std::unique_ptr<ResumableUploadSession> upload_session,
    std::size_t max_buffer_size, std::unique_ptr<HashValidator> hash_validator)
    : upload_session_(std::move(upload_session)),
      max_buffer_size_(UploadChunkRequest::RoundUpToQuantum(max_buffer_size)),
      hash_validator_(std::move(hash_validator)),
      last_response_{400} {
  current_ios_buffer_.reserve(max_buffer_size_);
}

bool CurlResumableStreambuf::IsOpen() const {
  return static_cast<bool>(upload_session_);
}

void CurlResumableStreambuf::ValidateHash(ObjectMetadata const& meta) {
  hash_validator_->ProcessMetadata(meta);
  hash_validator_result_ =
      HashValidator::FinishAndCheck(__func__, std::move(*hash_validator_));
}

CurlResumableStreambuf::int_type CurlResumableStreambuf::overflow(int_type ch) {
  Validate(__func__);
  Flush(false);
  if (not traits_type::eq_int_type(ch, traits_type::eof())) {
    current_ios_buffer_.push_back(traits_type::to_char_type(ch));
    pbump(1);
  }
  return 0;
}

int CurlResumableStreambuf::sync() {
  Flush(true);
  return 0;
}

std::streamsize CurlResumableStreambuf::xsputn(char const* s,
                                               std::streamsize count) {
  Validate(__func__);
  Flush(false);
  current_ios_buffer_.append(s, static_cast<std::size_t>(count));
  pbump(static_cast<int>(count));
  return count;
}

HttpResponse CurlResumableStreambuf::DoClose() {
  GCP_LOG(INFO) << __func__ << "()";
  return Flush(true);
}

void CurlResumableStreambuf::Validate(char const* where) const {
  if (IsOpen()) {
    return;
  }
  std::string msg = "Attempting to use closed CurlResumableStreambuf in ";
  msg += where;
  google::cloud::internal::RaiseRuntimeError(msg);
}

HttpResponse CurlResumableStreambuf::Flush(bool final_chunk) {
  if (not IsOpen()) {
    return last_response_;
  }
  // Shorten the buffer to the actual used size.
  auto actual_size = static_cast<std::size_t>(pptr() - pbase());
  if (actual_size == 0) {
    return last_response_;
  }
  if (actual_size <= max_buffer_size_ and not final_chunk) {
    return last_response_;
  }

  std::string trailing;
  std::size_t upload_size = 0U;
  if (final_chunk) {
    current_ios_buffer_.resize(actual_size);
    upload_size =
        upload_session_->next_expected_byte() + current_ios_buffer_.size();
  } else {
    trailing = current_ios_buffer_.substr(max_buffer_size_);
    current_ios_buffer_.resize(max_buffer_size_);
  }
  hash_validator_->Update(current_ios_buffer_);

  auto result = upload_session_->UploadChunk(current_ios_buffer_, upload_size);
  current_ios_buffer_.clear();
  current_ios_buffer_.reserve(max_buffer_size_);
  setp(&current_ios_buffer_[0], &current_ios_buffer_[0] + max_buffer_size_);
  current_ios_buffer_.append(trailing);
  pbump(static_cast<int>(trailing.size()));

  if (final_chunk) {
    upload_session_.reset();
  }

  last_response_ = HttpResponse{
      result.first.status_code(), std::move(result.second.payload), {}};
  return last_response_;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

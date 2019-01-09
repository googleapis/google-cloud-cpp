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

#include "google/cloud/storage/internal/curl_streambuf.h"
#include "google/cloud/storage/object_stream.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

CurlReadStreambuf::CurlReadStreambuf(
    CurlDownloadRequest&& download, std::size_t target_buffer_size,
    std::unique_ptr<HashValidator> hash_validator)
    : download_(std::move(download)),
      target_buffer_size_(target_buffer_size),
      hash_validator_(std::move(hash_validator)) {
  // Start with an empty read area, to force an underflow() on the first
  // extraction.
  current_ios_buffer_.push_back('\0');
  char* data = &current_ios_buffer_[0];
  setg(data, data + 1, data + 1);
}

bool CurlReadStreambuf::IsOpen() const { return download_.IsOpen(); }

void CurlReadStreambuf::Close() {
  auto response = download_.Close();
  if (not response.ok()) {
    status_ = std::move(response).status();
    ReportError(status_);
  }
}

CurlReadStreambuf::int_type CurlReadStreambuf::underflow() {
  char const* function_name = __func__;
  auto report_hash_mismatch = [this,
                               function_name]() -> CurlReadStreambuf::int_type {
    std::string msg;
    msg += function_name;
    msg += "() - mismatched hashes in download";
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    throw HashMismatchError(msg, hash_validator_result_.received,
                            hash_validator_result_.computed);
#else
    msg += ", expected=";
    msg += hash_validator_result_.computed;
    msg += ", received=";
    msg += hash_validator_result_.received;
    status_ = Status(StatusCode::kDataLoss, std::move(msg));
    return traits_type::eof();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  };

  if (not IsOpen()) {
    // The stream is closed, reading from a closed stream can happen if there is
    // no object to read from, or the object is empty. In that case just setup
    // an empty (but valid) region and verify the checksums.
    SetEmptyRegion();
    hash_validator_result_ = std::move(*hash_validator_).Finish();
    if (hash_validator_result_.is_mismatch) {
      return report_hash_mismatch();
    }
    return traits_type::eof();
  }

  current_ios_buffer_.reserve(target_buffer_size_);
  StatusOr<HttpResponse> response = download_.GetMore(current_ios_buffer_);
  if (not response.ok()) {
    return ReportError(std::move(response).status());
  }
  for (auto const& kv : response->headers) {
    hash_validator_->ProcessHeader(kv.first, kv.second);
    headers_.emplace(kv.first, kv.second);
  }
  if (response->status_code >= 300) {
    return ReportError(AsStatus(*response));
  }

  if (not current_ios_buffer_.empty()) {
    hash_validator_->Update(current_ios_buffer_);
    char* data = &current_ios_buffer_[0];
    setg(data, data, data + current_ios_buffer_.size());
    return traits_type::to_int_type(*data);
  }

  // This is an actual EOF, there is no more data to download, create an
  // empty (but valid) region:
  SetEmptyRegion();
  // Verify the checksums, and return the EOF character.
  hash_validator_result_ = std::move(*hash_validator_).Finish();
  if (hash_validator_result_.is_mismatch) {
    return report_hash_mismatch();
  }
  return traits_type::eof();
}

CurlReadStreambuf::int_type CurlReadStreambuf::ReportError(Status status) {
  // The only way to report errors from a std::basic_streambuf<> (which this
  // class derives from) is to throw exceptions:
  //   https://stackoverflow.com/questions/50716688/how-to-set-the-badbit-of-a-stream-by-a-customized-streambuf
  // but we need to be able to report errors when the application has disabled
  // exceptions via `-fno-exceptions` or a similar option. In that case we set
  // `status_`, and report the error as an EOF. This is obviously not ideal,
  // but it is the best we can do when the application disables the standard
  // mechanism to signal errors.
  status_ = std::move(status);
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  google::cloud::internal::ThrowStatus(status_);
#else
  return traits_type::eof();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

void CurlReadStreambuf::SetEmptyRegion() {
  current_ios_buffer_.clear();
  current_ios_buffer_.push_back('\0');
  char* data = &current_ios_buffer_[0];
  setg(data, data + 1, data + 1);
}

CurlWriteStreambuf::CurlWriteStreambuf(
    CurlUploadRequest&& upload, std::size_t max_buffer_size,
    std::unique_ptr<HashValidator> hash_validator)
    : upload_(std::move(upload)),
      max_buffer_size_(max_buffer_size),
      hash_validator_(std::move(hash_validator)) {
  current_ios_buffer_.reserve(max_buffer_size);
}

bool CurlWriteStreambuf::IsOpen() const { return upload_.IsOpen(); }

bool CurlWriteStreambuf::ValidateHash(ObjectMetadata const& meta) {
  hash_validator_->ProcessMetadata(meta);
  hash_validator_result_ = std::move(*hash_validator_).Finish();
  return not hash_validator_result_.is_mismatch;
}

CurlWriteStreambuf::int_type CurlWriteStreambuf::overflow(int_type ch) {
  Validate(__func__);
  SwapBuffers();
  if (not traits_type::eq_int_type(ch, traits_type::eof())) {
    current_ios_buffer_.push_back(traits_type::to_char_type(ch));
    pbump(1);
  }
  return 0;
}

int CurlWriteStreambuf::sync() {
  // The default destructor calls sync(), for already closed streams this should
  // be a no-op.
  if (not IsOpen()) {
    return 0;
  }
  SwapBuffers();
  upload_.Flush();
  return 0;
}

std::streamsize CurlWriteStreambuf::xsputn(char const* s,
                                           std::streamsize count) {
  Validate(__func__);
  current_ios_buffer_.append(s, static_cast<std::size_t>(count));
  pbump(static_cast<int>(count));
  if (current_ios_buffer_.size() > max_buffer_size_) {
    SwapBuffers();
  }
  return count;
}

StatusOr<HttpResponse> CurlWriteStreambuf::DoClose() {
  GCP_LOG(DEBUG) << __func__ << "()";
  Status status = Validate(__func__);
  if (not status.ok()) {
    return status;
  }
  status = SwapBuffers();
  if (not status.ok()) {
    return status;
  }
  auto response = upload_.Close();
  if (response.ok()) {
    for (auto const& kv : response->headers) {
      hash_validator_->ProcessHeader(kv.first, kv.second);
    }
  }
  return response;
}

Status CurlWriteStreambuf::Validate(char const* where) const {
  if (upload_.IsOpen()) {
    return Status();
  }
  std::string msg = "Attempting to use closed CurlStream in ";
  msg += where;
  return Status(StatusCode::kFailedPrecondition, std::move(msg));
}

Status CurlWriteStreambuf::SwapBuffers() {
  // Shorten the buffer to the actual used size.
  current_ios_buffer_.resize(pptr() - pbase());
  // Push the buffer to the libcurl wrapper to be written as needed
  hash_validator_->Update(current_ios_buffer_);
  auto status = upload_.NextBuffer(current_ios_buffer_);
  if (not status.ok()) {
    return status;
  }
  // Make the buffer big enough to receive more data before needing a flush.
  current_ios_buffer_.clear();
  current_ios_buffer_.reserve(max_buffer_size_);
  setp(&current_ios_buffer_[0], &current_ios_buffer_[0] + max_buffer_size_);
  return Status();
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

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

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

CurlStreambuf::CurlStreambuf(CurlUploadRequest&& upload,
                             std::size_t max_buffer_size)
    : upload_(std::move(upload)), max_buffer_size_(max_buffer_size) {
  current_ios_buffer_.reserve(max_buffer_size);
}

bool CurlStreambuf::IsOpen() const { return upload_.IsOpen(); }

CurlStreambuf::int_type CurlStreambuf::overflow(int_type ch) {
  Validate(__func__);
  SwapBuffers();
  if (not traits_type::eq_int_type(ch, traits_type::eof())) {
    current_ios_buffer_.push_back(traits_type::to_char_type(ch));
    pbump(1);
  }
  return 0;
}

int CurlStreambuf::sync() {
  // The default destructor calls sync(), for already closed streams this should
  // be a no-op.
  if (not IsOpen()) {
    return 0;
  }
  SwapBuffers();
  upload_.Flush();
  return 0;
}

std::streamsize CurlStreambuf::xsputn(char const* s, std::streamsize count) {
  Validate(__func__);
  current_ios_buffer_.append(s, static_cast<std::size_t>(count));
  pbump(static_cast<int>(count));
  if (current_ios_buffer_.size() > max_buffer_size_) {
    SwapBuffers();
  }
  return count;
}

HttpResponse CurlStreambuf::DoClose() {
  GCP_LOG(INFO) << __func__ << "()";
  Validate(__func__);
  SwapBuffers();
  return upload_.Close();
}

void CurlStreambuf::Validate(char const* where) const {
  if (upload_.IsOpen()) {
    return;
  }
  std::string msg = "Attempting to use closed CurlStream in ";
  msg += where;
  google::cloud::internal::RaiseRuntimeError(msg);
}

void CurlStreambuf::SwapBuffers() {
  // Shorten the buffer to the actual used size.
  current_ios_buffer_.resize(pptr() - pbase());
  // Push the buffer to the libcurl wrapper to be written as needed
  upload_.NextBuffer(current_ios_buffer_);
  // Make the buffer big enough to receive more data before needing a flush.
  current_ios_buffer_.clear();
  current_ios_buffer_.reserve(max_buffer_size_);
  setp(&current_ios_buffer_[0], &current_ios_buffer_[0] + max_buffer_size_);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

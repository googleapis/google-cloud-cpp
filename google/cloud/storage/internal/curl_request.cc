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

#include "google/cloud/storage/internal/curl_request.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

extern "C" size_t CurlRequestOnWriteData(char* ptr, size_t size, size_t nmemb,
                                         void* userdata) {
  auto* request = reinterpret_cast<CurlRequest*>(userdata);
  return request->OnWriteData(ptr, size, nmemb);
}

extern "C" size_t CurlRequestOnHeaderData(char* contents, size_t size,
                                          size_t nitems, void* userdata) {
  auto* request = reinterpret_cast<CurlRequest*>(userdata);
  return request->OnHeaderData(contents, size, nitems);
}

class WriteVector {
 public:
  explicit WriteVector(ConstBufferSequence w) : writev_(std::move(w)) {}

  bool empty() const { return writev_.empty(); }

  std::size_t OnRead(char* ptr, std::size_t size, std::size_t nitems) {
    std::size_t offset = 0;
    auto capacity = size * nitems;
    while (capacity > 0 && !writev_.empty()) {
      auto& f = writev_.front();
      auto n = (std::min)(capacity, writev_.front().size());
      std::copy(f.data(), f.data() + n, ptr + offset);
      offset += n;
      capacity -= n;
      PopFrontBytes(writev_, n);
    }
    return offset;
  }

 private:
  ConstBufferSequence writev_;
};

extern "C" std::size_t CurlRequestOnReadData(char* ptr, std::size_t size,
                                             std::size_t nitems,
                                             void* userdata) {
  auto* v = reinterpret_cast<WriteVector*>(userdata);
  return v->OnRead(ptr, size, nitems);
}

StatusOr<HttpResponse> CurlRequest::MakeRequest(std::string const& payload) {
  auto status = handle_.SetOption(CURLOPT_UPLOAD, 0L);
  if (!status.ok()) return status;
  if (!payload.empty()) {
    status = handle_.SetOption(CURLOPT_POSTFIELDSIZE, payload.length());
    if (!status.ok()) return status;
    status = handle_.SetOption(CURLOPT_POSTFIELDS, payload.c_str());
    if (!status.ok()) return status;
  }
  return MakeRequestImpl();
}

StatusOr<HttpResponse> CurlRequest::MakeUploadRequest(
    ConstBufferSequence payload) {
  auto status = handle_.SetOption(CURLOPT_UPLOAD, 0L);
  if (payload.empty()) return MakeRequestImpl();
  if (payload.size() == 1) {
    status = handle_.SetOption(CURLOPT_POSTFIELDSIZE, payload[0].size());
    if (!status.ok()) return status;
    status = handle_.SetOption(CURLOPT_POSTFIELDS, payload[0].data());
    if (!status.ok()) return status;
    return MakeRequestImpl();
  }

  WriteVector writev{std::move(payload)};
  status = handle_.SetOption(CURLOPT_READFUNCTION, &CurlRequestOnReadData);
  if (!status.ok()) return status;
  status = handle_.SetOption(CURLOPT_READDATA, &writev);
  if (!status.ok()) return status;
  status = handle_.SetOption(CURLOPT_UPLOAD, 1L);
  if (!status.ok()) return status;
  return MakeRequestImpl();
}

StatusOr<HttpResponse> CurlRequest::MakeRequestImpl() {
  // We get better performance using a slightly larger buffer (128KiB) than the
  // default buffer size set by libcurl (16KiB)
  auto constexpr kDefaultBufferSize = 128 * 1024L;

  response_payload_.clear();
  auto status = handle_.SetOption(CURLOPT_BUFFERSIZE, kDefaultBufferSize);
  if (!status.ok()) return status;
  status = handle_.SetOption(CURLOPT_URL, url_.c_str());
  if (!status.ok()) return status;
  status = handle_.SetOption(CURLOPT_HTTPHEADER, headers_.get());
  if (!status.ok()) return status;
  status = handle_.SetOption(CURLOPT_USERAGENT, user_agent_.c_str());
  if (!status.ok()) return status;
  status = handle_.SetOption(CURLOPT_NOSIGNAL, 1);
  if (!status.ok()) return status;
  status = handle_.SetOption(CURLOPT_TCP_KEEPALIVE, 1L);
  handle_.EnableLogging(logging_enabled_);
  handle_.SetSocketCallback(socket_options_);
  if (!status.ok()) return status;
  status = handle_.SetOption(CURLOPT_WRITEFUNCTION, &CurlRequestOnWriteData);
  if (!status.ok()) return status;
  status = handle_.SetOption(CURLOPT_WRITEDATA, this);
  if (!status.ok()) return status;
  status = handle_.SetOption(CURLOPT_HEADERFUNCTION, &CurlRequestOnHeaderData);
  if (!status.ok()) return status;
  status = handle_.SetOption(CURLOPT_HEADERDATA, this);
  if (!status.ok()) return status;
  status = handle_.EasyPerform();
  if (!status.ok()) return status;

  if (logging_enabled_) handle_.FlushDebug(__func__);
  auto code = handle_.GetResponseCode();
  if (!code.ok()) return std::move(code).status();
  return HttpResponse{code.value(), std::move(response_payload_),
                      std::move(received_headers_)};
}

std::size_t CurlRequest::OnWriteData(char* contents, std::size_t size,
                                     std::size_t nmemb) {
  response_payload_.append(contents, size * nmemb);
  return size * nmemb;
}

std::size_t CurlRequest::OnHeaderData(char* contents, std::size_t size,
                                      std::size_t nitems) {
  return CurlAppendHeaderData(received_headers_, contents, size * nitems);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

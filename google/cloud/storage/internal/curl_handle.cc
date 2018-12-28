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

#include "google/cloud/storage/internal/curl_handle.h"
#include "google/cloud/log.h"
#include "google/cloud/storage/internal/binary_data_as_debug_string.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using google::cloud::storage::internal::BinaryDataAsDebugString;

std::size_t const MAX_DATA_DEBUG_SIZE = 48;

extern "C" int CurlHandleDebugCallback(CURL*, curl_infotype type,
                                       char* data, std::size_t size,
                                       void* userptr) {
  auto debug_buffer = reinterpret_cast<std::string*>(userptr);
  switch (type) {
    case CURLINFO_TEXT:
      *debug_buffer += "== curl(Info): " + std::string(data, size);
      break;
    case CURLINFO_HEADER_IN:
      *debug_buffer += "<< curl(Recv Header): " + std::string(data, size);
      break;
    case CURLINFO_HEADER_OUT:
      *debug_buffer += ">> curl(Send Header): " + std::string(data, size);
      break;
    case CURLINFO_DATA_IN:
      *debug_buffer += ">> curl(Recv Data): size=";
      *debug_buffer += std::to_string(size) + "\n";
      *debug_buffer += BinaryDataAsDebugString(data, size, MAX_DATA_DEBUG_SIZE);
      break;
    case CURLINFO_DATA_OUT:
      *debug_buffer += ">> curl(Send Data): size=";
      *debug_buffer += std::to_string(size) + "\n";
      *debug_buffer += BinaryDataAsDebugString(data, size, MAX_DATA_DEBUG_SIZE);
      break;
    case CURLINFO_SSL_DATA_IN:
    case CURLINFO_SSL_DATA_OUT:
      // Do not print SSL binary data because generally that is not useful.
    case CURLINFO_END:
      break;
  }
  return 0;
}

extern "C" std::size_t CurlHandleReadCallback(char* ptr, std::size_t size,
                                              std::size_t nmemb,
                                              void* userdata) {
  auto* callback = reinterpret_cast<CurlHandle::ReaderCallback*>(userdata);
  return callback->operator()(ptr, size, nmemb);
}

extern "C" std::size_t CurlHandleWriteCallback(void* contents, std::size_t size,
                                               std::size_t nmemb,
                                               void* userdata) {
  auto* callback = reinterpret_cast<CurlHandle::WriterCallback*>(userdata);
  return callback->operator()(contents, size, nmemb);
}

extern "C" std::size_t CurlHandleHeaderCallback(char* contents,
                                                std::size_t size,
                                                std::size_t nitems,
                                                void* userdata) {
  auto* callback = reinterpret_cast<CurlHandle::HeaderCallback*>(userdata);
  return callback->operator()(contents, size, nitems);
}
}  // namespace

CurlHandle::CurlHandle() : handle_(curl_easy_init(), &curl_easy_cleanup) {
  if (handle_.get() == nullptr) {
    google::cloud::internal::RaiseRuntimeError("Cannot initialize CURL handle");
  }
}

CurlHandle::~CurlHandle() { FlushDebug(__func__); }

void CurlHandle::SetReaderCallback(ReaderCallback callback) {
  reader_callback_ = std::move(callback);
  SetOption(CURLOPT_READDATA, &reader_callback_);
  SetOption(CURLOPT_READFUNCTION, &CurlHandleReadCallback);
}

void CurlHandle::ResetReaderCallback() {
  SetOption(CURLOPT_READDATA, nullptr);
  SetOption(CURLOPT_READFUNCTION, nullptr);
  reader_callback_ = ReaderCallback();
}

void CurlHandle::SetWriterCallback(WriterCallback callback) {
  writer_callback_ = std::move(callback);
  SetOption(CURLOPT_WRITEDATA, &writer_callback_);
  SetOption(CURLOPT_WRITEFUNCTION, &CurlHandleWriteCallback);
}

void CurlHandle::ResetWriterCallback() {
  SetOption(CURLOPT_WRITEDATA, nullptr);
  SetOption(CURLOPT_WRITEFUNCTION, nullptr);
  writer_callback_ = WriterCallback();
}

void CurlHandle::SetHeaderCallback(HeaderCallback callback) {
  header_callback_ = std::move(callback);
  SetOption(CURLOPT_HEADERDATA, &header_callback_);
  SetOption(CURLOPT_HEADERFUNCTION, &CurlHandleHeaderCallback);
}

void CurlHandle::ResetHeaderCallback() {
  SetOption(CURLOPT_HEADERDATA, nullptr);
  SetOption(CURLOPT_HEADERFUNCTION, nullptr);
  header_callback_ = HeaderCallback();
}

void CurlHandle::EnableLogging(bool enabled) {
  if (enabled) {
    SetOption(CURLOPT_DEBUGDATA, &debug_buffer_);
    SetOption(CURLOPT_DEBUGFUNCTION, &CurlHandleDebugCallback);
    SetOption(CURLOPT_VERBOSE, 1L);
  } else {
    SetOption(CURLOPT_DEBUGDATA, nullptr);
    SetOption(CURLOPT_DEBUGFUNCTION, nullptr);
    SetOption(CURLOPT_VERBOSE, 0L);
  }
}

void CurlHandle::FlushDebug(char const* where) {
  if (not debug_buffer_.empty()) {
    GCP_LOG(DEBUG) << where << ' ' << debug_buffer_;
    debug_buffer_.clear();
  }
}

Status CurlHandle::AsStatus(CURLcode e, char const* where) {
  if (e == CURLE_OK) {
    return Status();
  }
  std::ostringstream os;
  os << where << "() - CURL error [" << e << "]=" << curl_easy_strerror(e);
  return Status(StatusCode::UNKNOWN, std::move(os).str());
}

void CurlHandle::RaiseSetOptionError(CURLcode e, CURLoption opt, long param) {
  std::ostringstream os;
  os << "Error [" << e << "]=" << curl_easy_strerror(e)
     << " while setting curl option [" << opt << "] to " << param;
  google::cloud::internal::RaiseRuntimeError(os.str());
}

void CurlHandle::RaiseSetOptionError(CURLcode e, CURLoption opt,
                                     char const* param) {
  std::ostringstream os;
  os << "Error [" << e << "]=" << curl_easy_strerror(e)
     << " while setting curl option [" << opt << "] to " << param;
  google::cloud::internal::RaiseRuntimeError(os.str());
}

void CurlHandle::RaiseSetOptionError(CURLcode e, CURLoption opt, void* param) {
  std::ostringstream os;
  os << "Error [" << e << "]=" << curl_easy_strerror(e)
     << " while setting curl option [" << opt << "] to " << param;
  google::cloud::internal::RaiseRuntimeError(os.str());
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

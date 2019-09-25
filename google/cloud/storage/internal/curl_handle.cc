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
#include <string.h>  // NOLINT
#ifdef _WIN32
#include <winsock.h>
#else
#include <sys/socket.h>
#endif  // _WIN32

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::storage::internal::BinaryDataAsDebugString;

std::size_t const kMaxDataDebugSize = 48;

extern "C" int CurlHandleDebugCallback(CURL*, curl_infotype type, char* data,
                                       std::size_t size, void* userptr) {
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
      *debug_buffer += BinaryDataAsDebugString(data, size, kMaxDataDebugSize);
      break;
    case CURLINFO_DATA_OUT:
      *debug_buffer += ">> curl(Send Data): size=";
      *debug_buffer += std::to_string(size) + "\n";
      *debug_buffer += BinaryDataAsDebugString(data, size, kMaxDataDebugSize);
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

extern "C" int CurlSetSocketOptions(void* userdata, curl_socket_t curlfd,
                                    curlsocktype purpose) {
  auto* options = reinterpret_cast<CurlHandle::SocketOptions*>(userdata);
  switch (purpose) {
    case CURLSOCKTYPE_IPCXN:
      // An option value of zero (the default) means "do not change the buffer
      // size", this is reasonable because 0 is an invalid value anyway.
      if (options->recv_buffer_size_ != 0) {
        auto size = static_cast<long>(options->recv_buffer_size_);
#if _WIN32
        int r = setsockopt(curlfd, SOL_SOCKET, SO_RCVBUF,
                           reinterpret_cast<char const*>(&size), sizeof(size));
#else
        int r = setsockopt(curlfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
#endif  // WIN32
        if (r != 0) {
          GCP_LOG(ERROR) << __func__
                         << "(): setting socket recv buffer size to " << size
                         << " error=" << ::strerror(errno) << " [" << errno
                         << "]";
          return CURL_SOCKOPT_ERROR;
        }
      }
      if (options->send_buffer_size_ != 0) {
        auto size = static_cast<long>(options->send_buffer_size_);
#if _WIN32
        int r = setsockopt(curlfd, SOL_SOCKET, SO_SNDBUF,
                           reinterpret_cast<char const*>(&size), sizeof(size));
#else
        auto r = setsockopt(curlfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
#endif  // WIN32
        if (r != 0) {
          GCP_LOG(ERROR) << __func__
                         << "(): setting socket send buffer size to " << size
                         << " error=" << ::strerror(errno) << " [" << errno
                         << "]";
          return CURL_SOCKOPT_ERROR;
        }
      }
      break;
    case CURLSOCKTYPE_ACCEPT:
    case CURLSOCKTYPE_LAST:
      break;
  }
  return CURL_SOCKOPT_OK;
}

}  // namespace

CurlHandle::CurlHandle() : handle_(curl_easy_init(), &curl_easy_cleanup) {
  if (handle_.get() == nullptr) {
    google::cloud::internal::ThrowRuntimeError("Cannot initialize CURL handle");
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

void CurlHandle::SetSocketCallback(SocketOptions const& options) {
  socket_options_ = options;
  SetOption(CURLOPT_SOCKOPTDATA, &socket_options_);
  SetOption(CURLOPT_SOCKOPTFUNCTION, &CurlSetSocketOptions);
}

void CurlHandle::ResetSocketCallback() {
  SetOption(CURLOPT_SOCKOPTDATA, nullptr);
  SetOption(CURLOPT_SOCKOPTFUNCTION, nullptr);
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
  if (!debug_buffer_.empty()) {
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
  // Map the CURLE* errors using the documentation on:
  //   https://curl.haxx.se/libcurl/c/libcurl-errors.html
  StatusCode code;
  switch (e) {
    case CURLE_COULDNT_RESOLVE_PROXY:
    case CURLE_COULDNT_RESOLVE_HOST:
    case CURLE_COULDNT_CONNECT:
    case CURLE_RECV_ERROR:
    case CURLE_SEND_ERROR:
    case CURLE_PARTIAL_FILE:
    case CURLE_SSL_CONNECT_ERROR:
      code = StatusCode::kUnavailable;
      break;
    case CURLE_REMOTE_ACCESS_DENIED:
      code = StatusCode::kPermissionDenied;
      break;
    case CURLE_OPERATION_TIMEDOUT:
      code = StatusCode::kDeadlineExceeded;
      break;
    case CURLE_RANGE_ERROR:
      // This is defined as "the server does not *support or *accept* range
      // requests", so it means something stronger than "your range value is
      // not valid".
      code = StatusCode::kUnimplemented;
      break;
    case CURLE_BAD_DOWNLOAD_RESUME:
      code = StatusCode::kInvalidArgument;
      break;
    case CURLE_ABORTED_BY_CALLBACK:
      code = StatusCode::kAborted;
      break;
    case CURLE_REMOTE_FILE_NOT_FOUND:
      code = StatusCode::kNotFound;
      break;
    default:
      // There are ~82 error codes, some are not applicable (CURLE_FTP*), some
      // of them are not available on all versions, and some are explicitly
      // marked as obsolete. Instead of listing all of them, just default to
      // kUnknown.
      code = StatusCode::kUnknown;
      break;
  }
  return Status(code, std::move(os).str());
}

void CurlHandle::ThrowSetOptionError(CURLcode e, CURLoption opt, long param) {
  std::ostringstream os;
  os << "Error [" << e << "]=" << curl_easy_strerror(e)
     << " while setting curl option [" << opt << "] to " << param;
  google::cloud::internal::ThrowRuntimeError(os.str());
}

void CurlHandle::ThrowSetOptionError(CURLcode e, CURLoption opt,
                                     char const* param) {
  std::ostringstream os;
  os << "Error [" << e << "]=" << curl_easy_strerror(e)
     << " while setting curl option [" << opt << "] to " << param;
  google::cloud::internal::ThrowRuntimeError(os.str());
}

void CurlHandle::ThrowSetOptionError(CURLcode e, CURLoption opt, void* param) {
  std::ostringstream os;
  os << "Error [" << e << "]=" << curl_easy_strerror(e)
     << " while setting curl option [" << opt << "] to " << param;
  google::cloud::internal::ThrowRuntimeError(os.str());
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

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
#include "google/cloud/storage/internal/binary_data_as_debug_string.h"
#include "google/cloud/internal/strerror.h"
#include "google/cloud/log.h"
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

extern "C" int CurlSetSocketOptions(void* userdata, curl_socket_t curlfd,
                                    curlsocktype purpose) {
  auto errno_msg = [] { return google::cloud::internal::strerror(errno); };
  auto* options = reinterpret_cast<CurlHandle::SocketOptions*>(userdata);
  switch (purpose) {
    case CURLSOCKTYPE_IPCXN:
      // An option value of zero (the default) means "do not change the buffer
      // size", this is reasonable because 0 is an invalid value anyway.
      if (options->recv_buffer_size_ != 0) {
        // NOLINTNEXTLINE(google-runtime-int) - setsockopt *requires* `long`
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
                         << " error=" << errno_msg() << " [" << errno << "]";
          return CURL_SOCKOPT_ERROR;
        }
      }
      if (options->send_buffer_size_ != 0) {
        // NOLINTNEXTLINE(google-runtime-int) - setsockopt *requires* `long`
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
                         << " error=" << errno_msg() << " [" << errno << "]";
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
  // The error codes are listed in the same order as shown on that page, so
  // one can quickly find out how an error code is handled. All the error codes
  // are listed, but those that do not appear in old libcurl versions are
  // commented out and handled by the `default:` case.
  StatusCode code;
  switch (e) {
    case CURLE_UNSUPPORTED_PROTOCOL:
    case CURLE_FAILED_INIT:
    case CURLE_URL_MALFORMAT:
    case CURLE_NOT_BUILT_IN:
      code = StatusCode::kUnknown;
      break;

    case CURLE_COULDNT_RESOLVE_PROXY:
    case CURLE_COULDNT_RESOLVE_HOST:
    case CURLE_COULDNT_CONNECT:
      code = StatusCode::kUnavailable;
      break;

    // missing in some older libcurl versions:   CURLE_WEIRD_SERVER_REPLY
    case CURLE_REMOTE_ACCESS_DENIED:
      code = StatusCode::kPermissionDenied;
      break;

    case CURLE_FTP_ACCEPT_FAILED:
    case CURLE_FTP_WEIRD_PASS_REPLY:
    case CURLE_FTP_WEIRD_227_FORMAT:
    case CURLE_FTP_CANT_GET_HOST:
    // missing in some older libcurl versions:   CURLE_HTTP2
    case CURLE_FTP_COULDNT_SET_TYPE:
      code = StatusCode::kUnknown;
      break;

    case CURLE_PARTIAL_FILE:
      code = StatusCode::kUnavailable;
      break;

    case CURLE_FTP_COULDNT_RETR_FILE:
    case CURLE_QUOTE_ERROR:
    case CURLE_WRITE_ERROR:
    case CURLE_UPLOAD_FAILED:
    case CURLE_READ_ERROR:
    case CURLE_OUT_OF_MEMORY:
      code = StatusCode::kUnknown;
      break;

    case CURLE_OPERATION_TIMEDOUT:
      code = StatusCode::kDeadlineExceeded;
      break;

    case CURLE_FTP_PORT_FAILED:
    case CURLE_FTP_COULDNT_USE_REST:
      code = StatusCode::kUnknown;
      break;

    case CURLE_RANGE_ERROR:
      // This is defined as "the server does not *support or *accept* range
      // requests", so it means something stronger than "your range value is
      // not valid".
      code = StatusCode::kUnimplemented;
      break;

    case CURLE_HTTP_POST_ERROR:
      code = StatusCode::kUnknown;
      break;

    case CURLE_SSL_CONNECT_ERROR:
      code = StatusCode::kUnavailable;
      break;

    case CURLE_BAD_DOWNLOAD_RESUME:
      code = StatusCode::kInvalidArgument;
      break;

    case CURLE_FILE_COULDNT_READ_FILE:
    case CURLE_LDAP_CANNOT_BIND:
    case CURLE_LDAP_SEARCH_FAILED:
    case CURLE_FUNCTION_NOT_FOUND:
      code = StatusCode::kUnknown;
      break;

    case CURLE_ABORTED_BY_CALLBACK:
      code = StatusCode::kAborted;
      break;

    case CURLE_BAD_FUNCTION_ARGUMENT:
    case CURLE_INTERFACE_FAILED:
    case CURLE_TOO_MANY_REDIRECTS:
    case CURLE_UNKNOWN_OPTION:
    case CURLE_TELNET_OPTION_SYNTAX:
      code = StatusCode::kUnknown;
      break;

    case CURLE_GOT_NOTHING:
      code = StatusCode::kUnavailable;
      break;

    case CURLE_SSL_ENGINE_NOTFOUND:
      code = StatusCode::kUnknown;
      break;

    case CURLE_RECV_ERROR:
    case CURLE_SEND_ERROR:
      code = StatusCode::kUnavailable;
      break;

    case CURLE_SSL_CERTPROBLEM:
    case CURLE_SSL_CIPHER:
    case CURLE_PEER_FAILED_VERIFICATION:
    case CURLE_BAD_CONTENT_ENCODING:
    case CURLE_LDAP_INVALID_URL:
    case CURLE_FILESIZE_EXCEEDED:
    case CURLE_USE_SSL_FAILED:
    case CURLE_SEND_FAIL_REWIND:
    case CURLE_SSL_ENGINE_SETFAILED:
    case CURLE_LOGIN_DENIED:
    case CURLE_TFTP_NOTFOUND:
    case CURLE_TFTP_PERM:
    case CURLE_REMOTE_DISK_FULL:
    case CURLE_TFTP_ILLEGAL:
    case CURLE_TFTP_UNKNOWNID:
    case CURLE_REMOTE_FILE_EXISTS:
    case CURLE_TFTP_NOSUCHUSER:
    case CURLE_CONV_FAILED:
    case CURLE_CONV_REQD:
    case CURLE_SSL_CACERT_BADFILE:
      code = StatusCode::kUnknown;
      break;

    case CURLE_REMOTE_FILE_NOT_FOUND:
      code = StatusCode::kNotFound;
      break;

      // NOLINTNEXTLINE(bugprone-branch-clone)
    case CURLE_SSH:
    case CURLE_SSL_SHUTDOWN_FAILED:
      code = StatusCode::kUnknown;
      break;

    case CURLE_AGAIN:
      // This looks like a good candidate for kUnavailable, but it is only
      // returned by curl_easy_{recv,send}, and should not with the
      // configuration we use for libcurl, and the recovery action is to call
      // curl_easy_{recv,send} again, which is not how this return value is used
      // (we restart the whole transfer).
      code = StatusCode::kUnknown;
      break;

    case CURLE_SSL_CRL_BADFILE:
    case CURLE_SSL_ISSUER_ERROR:
    case CURLE_FTP_PRET_FAILED:
    case CURLE_RTSP_CSEQ_ERROR:
    case CURLE_RTSP_SESSION_ERROR:
    case CURLE_FTP_BAD_FILE_LIST:
    case CURLE_CHUNK_FAILED:
      code = StatusCode::kUnknown;
      break;

    // missing in some older libcurl versions:   CURLE_HTTP_RETURNED_ERROR
    // missing in some older libcurl versions:   CURLE_NO_CONNECTION_AVAILABLE
    // missing in some older libcurl versions:   CURLE_SSL_PINNEDPUBKEYNOTMATCH
    // missing in some older libcurl versions:   CURLE_SSL_INVALIDCERTSTATUS
    // missing in some older libcurl versions:   CURLE_HTTP2_STREAM
    // missing in some older libcurl versions:   CURLE_RECURSIVE_API_CALL
    // missing in some older libcurl versions:   CURLE_AUTH_ERROR
    // missing in some older libcurl versions:   CURLE_HTTP3
    // missing in some older libcurl versions:   CURLE_QUIC_CONNECT_ERROR
    default:
      // As described above, there are about 100 error codes, some are
      // explicitly marked as obsolete, some are not available in all libcurl
      // versions. Use this `default:` case to treat all such errors as
      // `kUnknown`.
      code = StatusCode::kUnknown;
      break;
  }
  return Status(code, std::move(os).str());
}

void CurlHandle::ThrowSetOptionError(CURLcode e, CURLoption opt,
                                     std::intmax_t param) {
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

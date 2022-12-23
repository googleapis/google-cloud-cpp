// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/curl_impl.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/curl_options.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_options.h"
#include "google/cloud/internal/user_agent_prefix.h"
#include "google/cloud/log.h"
#include "absl/strings/match.h"
#include <algorithm>
#include <sstream>
#include <thread>

// Note that TRACE-level messages are disabled by default, even in
// CMAKE_BUILD_TYPE=Debug builds. The level of detail created by the
// TRACE_STATE() macro is only needed by the library developers when
// troubleshooting this class.
#define TRACE_STATE()                                                       \
  GCP_LOG(TRACE) << __func__ << "(), avail_.size()=" << avail_.size()       \
                 << ", spill_.capacity()=" << spill_.capacity()             \
                 << ", spill_.size()=" << spill_.size()                     \
                 << ", closing=" << closing_ << ", closed=" << curl_closed_ \
                 << ", paused=" << paused_ << ", in_multi=" << in_multi_

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

std::string UserAgentSuffix() {
  static auto const* const kUserAgentSuffix = new auto([] {
    return absl::StrCat(internal::UserAgentPrefix(), " ", curl_version());
  }());
  return *kUserAgentSuffix;
}

std::string NormalizeEndpoint(std::string endpoint) {
  if (!endpoint.empty() && endpoint.back() != '/') endpoint.push_back('/');
  return endpoint;
}

char const* InitialQueryParameterSeparator(std::string const& url) {
  // Abseil <= 20200923 does not implement StrContains(.., char)
  // NOLINTNEXTLINE(abseil-string-find-str-contains)
  if (url.find('?') != std::string::npos) return "&";
  return "?";
}

char const* HttpMethodToName(CurlImpl::HttpMethod method) {
  switch (method) {
    case CurlImpl::HttpMethod::kDelete:
      return "DELETE";
    case CurlImpl::HttpMethod::kGet:
      return "GET";
    case CurlImpl::HttpMethod::kPatch:
      return "PATCH";
    case CurlImpl::HttpMethod::kPost:
      return "POST";
    case CurlImpl::HttpMethod::kPut:
      return "PUT";
  }
  return "UNKNOWN";
}

// Convert a CURLM_* error code to a google::cloud::Status().
Status AsStatus(CURLMcode result, char const* where) {
  if (result == CURLM_OK) return {};
  std::ostringstream os;
  os << where << "() - CURL error [" << result
     << "]=" << curl_multi_strerror(result);
  return internal::UnknownError(std::move(os).str());
}

// Vector of data chunks to satisfy requests from libcurl.
class WriteVector {
 public:
  explicit WriteVector(std::vector<absl::Span<char const>> v)
      : writev_(std::move(v)) {
    // Reverse the vector so the first chunk is at the end.
    std::reverse(writev_.begin(), writev_.end());
  }

  std::size_t size() const {
    std::size_t size = 0;
    for (auto const& s : writev_) {
      size += s.size();
    }
    return size;
  }

  std::size_t MoveTo(absl::Span<char> dst) {
    auto const avail = dst.size();
    while (!writev_.empty()) {
      auto& src = writev_.back();
      if (src.size() > dst.size()) {
        std::copy(src.begin(), src.begin() + dst.size(), dst.begin());
        src.remove_prefix(dst.size());
        dst.remove_prefix(dst.size());
        break;
      }
      std::copy(src.begin(), src.end(), dst.begin());
      dst.remove_prefix(src.size());
      writev_.pop_back();
    }
    return avail - dst.size();
  }

 private:
  std::vector<absl::Span<char const>> writev_;
};

}  // namespace

std::size_t SpillBuffer::CopyFrom(absl::Span<char const> src) {
  // capacity() is CURL_MAX_WRITE_SIZE, the maximum amount of data that
  // libcurl will pass to CurlImpl::WriteCallback(). However, it can give
  // less data, resulting in multiple CopyFrom() calls on the initial read.
  if (src.size() > capacity() - size_) {
    GCP_LOG(FATAL) << "Attempted to write " << src.size()
                   << " bytes into SpillBuffer with only "
                   << (capacity() - size_) << " bytes available";
  }

  auto const len = src.size();
  auto end = start_ + size_;
  if (end >= capacity()) end -= capacity();
  if (end + len <= capacity()) {
    std::copy(src.begin(), src.end(), buffer_.begin() + end);
  } else {
    absl::Span<char>::const_iterator s = src.begin() + (capacity() - end);
    std::copy(src.begin(), s, buffer_.begin() + end);
    std::copy(s, src.end(), buffer_.begin());
  }
  size_ += len;
  return len;
}

std::size_t SpillBuffer::MoveTo(absl::Span<char> dst) {
  auto const len = (std::min)(size_, dst.size());
  auto const end = start_ + len;
  absl::Span<char>::iterator d = dst.begin();
  if (end <= capacity()) {
    std::copy(buffer_.begin() + start_, buffer_.begin() + end, d);
    start_ = (end == capacity()) ? 0 : end;
  } else {
    d = std::copy(buffer_.begin() + start_, buffer_.end(), d);
    start_ = end - capacity();
    std::copy(buffer_.begin(), buffer_.begin() + start_, d);
  }
  size_ -= len;
  if (size_ == 0) start_ = 0;  // defrag optimization
  return len;
}

extern "C" {  // libcurl callbacks

// It would be nice to be able to send data from, and receive data into,
// our own buffers (i.e., without an extra copy). But, there is no such API.

// Fill buffer to send data to peer (POST/PUT).
static std::size_t ReadFunction(char* buffer, std::size_t size,
                                std::size_t nitems, void* userdata) {
  auto* const writev = reinterpret_cast<WriteVector*>(userdata);
  return writev->MoveTo(absl::MakeSpan(buffer, size * nitems));
}

// Receive a response header from peer.
static std::size_t HeaderFunction(char* buffer, std::size_t size,
                                  std::size_t nitems, void* userdata) {
  auto* const request = reinterpret_cast<CurlImpl*>(userdata);
  return request->HeaderCallback(absl::MakeSpan(buffer, size * nitems));
}

// Receive response data from peer.
static std::size_t WriteFunction(char* ptr, size_t size, size_t nmemb,
                                 void* userdata) {
  auto* const request = reinterpret_cast<CurlImpl*>(userdata);
  return request->WriteCallback(absl::MakeSpan(ptr, size * nmemb));
}

}  // extern "C"

CurlImpl::CurlImpl(CurlHandle handle,
                   std::shared_ptr<CurlHandleFactory> factory,
                   Options const& options)
    : factory_(std::move(factory)),
      request_headers_(nullptr, &curl_slist_free_all),
      handle_(std::move(handle)),
      multi_(factory_->CreateMultiHandle()) {
  CurlInitializeOnce(options);

  logging_enabled_ = google::cloud::internal::Contains(
      options.get<TracingComponentsOption>(), "http");
  follow_location_ = options.get<CurlFollowLocationOption>();

  socket_options_.recv_buffer_size_ =
      options.get<MaximumCurlSocketRecvSizeOption>();
  socket_options_.send_buffer_size_ =
      options.get<MaximumCurlSocketSendSizeOption>();

  auto const& agents = options.get<UserAgentProductsOption>();
  user_agent_ = absl::StrCat(absl::StrJoin(agents, " "), UserAgentSuffix());

  http_version_ = options.get<HttpVersionOption>();

  transfer_stall_timeout_ = options.get<TransferStallTimeoutOption>();
  transfer_stall_minimum_rate_ = options.get<TransferStallMinimumRateOption>();
  download_stall_timeout_ = options.get<DownloadStallTimeoutOption>();
  download_stall_minimum_rate_ = options.get<DownloadStallMinimumRateOption>();
}

CurlImpl::~CurlImpl() {
  if (!curl_closed_) {
    // Set the closing_ flag to trigger a return 0 from the next
    // WriteCallback().  See the header file for more details.
    closing_ = true;
    TRACE_STATE();

    // Ignore errors. Except in some really unfortunate cases [*], we are
    // closing the download early. That is done [**] by having WriteCallback()
    // return 0, which always results in libcurl returning CURLE_WRITE_ERROR.
    //
    // [*] The only other case would be where a download completes and
    //   the handle is paused because just the right number of bytes
    //   arrived to satisfy the last Read() request. In that case ignoring
    //   the errors seems sensible too. The download completed, so what is
    //   the problem?
    //
    // [**] This is the recommended practice to shutdown a download early.
    //   See the comments in the header file and elsewhere in this file.
    (void)handle_.EasyPerform();
    curl_closed_ = true;
    TRACE_STATE();
  }

  CleanupHandles();

  CurlHandle::ReturnToPool(*factory_, std::move(handle_));
  factory_->CleanupMultiHandle(std::move(multi_), HandleDisposition::kKeep);
}

void CurlImpl::SetHeader(std::string const& header) {
  if (header.empty()) return;

  // The API for credentials is complicated, and the authorization
  // header can be empty. See, for example, AnonymousCredentials.
  if (header == "authorization: ") return;

  auto* headers = curl_slist_append(request_headers_.get(), header.c_str());
  (void)request_headers_.release();  // Now owned by list, not us.
  request_headers_.reset(headers);
}

void CurlImpl::SetHeader(std::pair<std::string, std::string> const& header) {
  SetHeader(absl::StrCat(header.first, ": ", header.second));
}

void CurlImpl::SetHeaders(RestRequest const& request) {
  for (auto const& header : request.headers()) {
    SetHeader(std::make_pair(header.first, absl::StrJoin(header.second, ",")));
  }
}

std::string CurlImpl::MakeEscapedString(std::string const& s) {
  return handle_.MakeEscapedString(s).get();
}

void CurlImpl::SetUrl(
    std::string const& endpoint, RestRequest const& request,
    RestRequest::HttpParameters const& additional_parameters) {
  if (request.path().empty() && additional_parameters.empty()) {
    url_ = endpoint;
    return;
  }

  if (absl::StartsWithIgnoreCase(request.path(), "http://") ||
      absl::StartsWithIgnoreCase(request.path(), "https://")) {
    url_ = request.path();
  } else {
    url_ = absl::StrCat(NormalizeEndpoint(endpoint), request.path());
  }

  char const* query_parameter_separator = InitialQueryParameterSeparator(url_);
  auto append_params = [&](RestRequest::HttpParameters const& parameters) {
    for (auto const& param : parameters) {
      if (param.first == "userIp" && param.second.empty()) {
        absl::StrAppend(&url_, query_parameter_separator,
                        MakeEscapedString(param.first), "=",
                        MakeEscapedString(LastClientIpAddress()));
      } else {
        absl::StrAppend(&url_, query_parameter_separator,
                        MakeEscapedString(param.first), "=",
                        MakeEscapedString(param.second));
      }
      query_parameter_separator = "&";
    }
  };
  append_params(request.parameters());
  append_params(additional_parameters);
}

std::string CurlImpl::LastClientIpAddress() const {
  return factory_->LastClientIpAddress();
}

Status CurlImpl::MakeRequest(HttpMethod method,
                             std::vector<absl::Span<char const>> request) {
  Status status;
  status = handle_.SetOption(CURLOPT_CUSTOMREQUEST, HttpMethodToName(method));
  if (!status.ok()) return OnTransferError(std::move(status));
  status = handle_.SetOption(CURLOPT_UPLOAD, 0L);
  if (!status.ok()) return OnTransferError(std::move(status));
  status =
      handle_.SetOption(CURLOPT_FOLLOWLOCATION, follow_location_ ? 1L : 0L);
  if (!status.ok()) return OnTransferError(std::move(status));

  if (method == HttpMethod::kGet) {
    status = handle_.SetOption(CURLOPT_NOPROGRESS, 1L);
    if (!status.ok()) return OnTransferError(std::move(status));
    if (download_stall_timeout_ != std::chrono::seconds::zero()) {
      // NOLINTNEXTLINE(google-runtime-int) - libcurl *requires* long
      auto const timeout = static_cast<long>(download_stall_timeout_.count());
      // NOLINTNEXTLINE(google-runtime-int) - libcurl *requires* long
      auto const limit = static_cast<long>(download_stall_minimum_rate_);
      status = handle_.SetOption(CURLOPT_CONNECTTIMEOUT, timeout);
      if (!status.ok()) return OnTransferError(std::move(status));
      // Timeout if the request sends or receives less than 1 byte/second
      // (i.e.  effectively no bytes) for download_stall_timeout_.
      status = handle_.SetOption(CURLOPT_LOW_SPEED_LIMIT, limit);
      if (!status.ok()) return OnTransferError(std::move(status));
      status = handle_.SetOption(CURLOPT_LOW_SPEED_TIME, timeout);
      if (!status.ok()) return OnTransferError(std::move(status));
    }
    return MakeRequestImpl();
  }

  if (transfer_stall_timeout_ != std::chrono::seconds::zero()) {
    // NOLINTNEXTLINE(google-runtime-int) - libcurl *requires* long
    auto const timeout = static_cast<long>(transfer_stall_timeout_.count());
    // NOLINTNEXTLINE(google-runtime-int) - libcurl *requires* long
    auto const limit = static_cast<long>(transfer_stall_minimum_rate_);
    status = handle_.SetOption(CURLOPT_CONNECTTIMEOUT, timeout);
    if (!status.ok()) return OnTransferError(std::move(status));
    // Timeout if the request sends or receives less than 1 byte/second
    // (i.e.  effectively no bytes) for transfer_stall_timeout_.
    status = handle_.SetOption(CURLOPT_LOW_SPEED_LIMIT, limit);
    if (!status.ok()) return OnTransferError(std::move(status));
    status = handle_.SetOption(CURLOPT_LOW_SPEED_TIME, timeout);
    if (!status.ok()) return OnTransferError(std::move(status));
  }

  if (method == HttpMethod::kDelete || request.empty()) {
    return MakeRequestImpl();
  }

  if (method == HttpMethod::kPost) {
    WriteVector writev{std::move(request)};
    curl_off_t const size = writev.size();
    status = handle_.SetOption(CURLOPT_POSTFIELDS, nullptr);
    if (!status.ok()) return OnTransferError(std::move(status));
    status = handle_.SetOption(CURLOPT_POST, 1L);
    if (!status.ok()) return OnTransferError(std::move(status));
    status = handle_.SetOption(CURLOPT_POSTFIELDSIZE_LARGE, size);
    if (!status.ok()) return OnTransferError(std::move(status));
    status = handle_.SetOption(CURLOPT_READFUNCTION, &ReadFunction);
    if (!status.ok()) return OnTransferError(std::move(status));
    status = handle_.SetOption(CURLOPT_READDATA, &writev);
    if (!status.ok()) return OnTransferError(std::move(status));
    SetHeader("Expect:");
    return MakeRequestImpl();
  }

  if (method == HttpMethod::kPut || method == HttpMethod::kPatch) {
    WriteVector writev{std::move(request)};
    curl_off_t const size = writev.size();
    status = handle_.SetOption(CURLOPT_READFUNCTION, &ReadFunction);
    if (!status.ok()) return OnTransferError(std::move(status));
    status = handle_.SetOption(CURLOPT_READDATA, &writev);
    if (!status.ok()) return OnTransferError(std::move(status));
    status = handle_.SetOption(CURLOPT_UPLOAD, 1L);
    if (!status.ok()) return OnTransferError(std::move(status));
    status = handle_.SetOption(CURLOPT_INFILESIZE_LARGE, size);
    if (!status.ok()) return OnTransferError(std::move(status));
    return MakeRequestImpl();
  }

  return internal::InvalidArgumentError(
      absl::StrCat("Unknown method: ", method));
}

bool CurlImpl::HasUnreadData() const {
  return !curl_closed_ || spill_.size() != 0;
}

StatusOr<std::size_t> CurlImpl::Read(absl::Span<char> output) {
  if (output.empty()) {
    return internal::InvalidArgumentError("Output buffer cannot be empty");
  }
  return ReadImpl(std::move(output));
}

std::size_t CurlImpl::WriteCallback(absl::Span<char> response) {
  handle_.FlushDebug(__func__);
  TRACE_STATE() << ", begin"
                << ", size=" << response.size();

  // This transfer is closing, so just return zero. That will make libcurl
  // finish any pending work, and will return the handle_ pointer from
  // curl_multi_info_read() in PerformWork(), where curl_closed_ is set.
  if (closing_) {
    TRACE_STATE() << ", closing";
    return 0;
  }

  // If headers have not been received and avail_ is empty then this is the
  // initial call to make the request, and we need to stash the received bytes
  // into the spill buffer so that we can make the response code and headers
  // available without requiring the user to read the response. Any bytes
  // sequestered in the spill buffer will be the first returned to the user
  // on attempts to read the response. Only after the spill buffer has been
  // emptied will we read more from handle_.
  if (!all_headers_received_ && avail_.empty()) {
    all_headers_received_ = true;
    http_code_ = static_cast<HttpStatusCode>(handle_.GetResponseCode());
    // Capture the peer (the HTTP server). Used for troubleshooting.
    received_headers_.emplace(":curl-peer", handle_.GetPeer());
    TRACE_STATE() << ", headers received";
    return spill_.CopyFrom(response);
  }

  // Use the spill buffer first.
  avail_.remove_prefix(spill_.MoveTo(avail_));

  // Check that we can accept all the data. If not, pause the transfer and
  // request that the data be delivered again when the transfer is unpaused.
  if (response.size() > avail_.size() + (spill_.capacity() - spill_.size())) {
    paused_ = true;
    TRACE_STATE() << ", response.size()=" << response.size()
                  << " too big *** PAUSING HANDLE ***";
    return CURL_WRITEFUNC_PAUSE;
  }

  // We're now committed to consuming the entire response.
  auto const response_size = response.size();

  // Copy as much as possible to the output.
  auto const len = (std::min)(response_size, avail_.size());
  std::copy(response.begin(), response.begin() + len, avail_.begin());
  response.remove_prefix(len);
  avail_.remove_prefix(len);

  // Copy the remainder to the spill buffer.
  spill_.CopyFrom(response);

  TRACE_STATE() << ", end";
  return response_size;
}

// libcurl invokes the HEADERFUNCTION exactly once for each complete header
// line received. The status line and blank lines preceding and following the
// headers are also passed to this function.
std::size_t CurlImpl::HeaderCallback(absl::Span<char> response) {
  return CurlAppendHeaderData(received_headers_, response.data(),
                              response.size());
}

Status CurlImpl::MakeRequestImpl() {
  TRACE_STATE() << ", url_=" << url_;

  Status status;
  status = handle_.SetOption(CURLOPT_URL, url_.c_str());
  if (!status.ok()) return OnTransferError(std::move(status));
  status = handle_.SetOption(CURLOPT_HTTPHEADER, request_headers_.get());
  if (!status.ok()) return OnTransferError(std::move(status));
  status = handle_.SetOption(CURLOPT_USERAGENT, user_agent_.c_str());
  if (!status.ok()) return OnTransferError(std::move(status));
  handle_.EnableLogging(logging_enabled_);
  if (!status.ok()) return OnTransferError(std::move(status));
  handle_.SetSocketCallback(socket_options_);
  if (!status.ok()) return OnTransferError(std::move(status));
  status = handle_.SetOption(CURLOPT_NOSIGNAL, 1);
  if (!status.ok()) return OnTransferError(std::move(status));
  status = handle_.SetOption(CURLOPT_TCP_KEEPALIVE, 1L);
  if (!status.ok()) return OnTransferError(std::move(status));

  handle_.SetOptionUnchecked(CURLOPT_HTTP_VERSION,
                             VersionToCurlCode(http_version_));

  auto error = curl_multi_add_handle(multi_.get(), handle_.handle_.get());

  // This indicates that we are using the API incorrectly. The application
  // can not recover from these problems, so terminating is the right thing
  // to do.
  if (error != CURLM_OK) {
    GCP_LOG(FATAL) << ", status=" << AsStatus(error, __func__);
  }

  in_multi_ = true;

  // This call to Read() should send the request, get the response, and
  // thus make available the status_code and headers. Any response data
  // should be put into the spill buffer, which makes them available for
  // subsequent calls to Read() after the headers have been extracted.
  return ReadImpl({}).status();
}

StatusOr<std::size_t> CurlImpl::ReadImpl(absl::Span<char> output) {
  handle_.FlushDebug(__func__);
  avail_ = output;
  TRACE_STATE() << ", begin";

  // Before calling WaitForHandles(), move any data from the spill buffer
  // into the output buffer. It is possible that WaitForHandles() will
  // never call WriteCallback() (e.g., because PerformWork() closed the
  // connection), but if there is any data left in the spill buffer we
  // need to return it.
  auto bytes_read = spill_.MoveTo(avail_);
  avail_.remove_prefix(bytes_read);
  if (curl_closed_) return bytes_read;

  Status status;
  status = handle_.SetOption(CURLOPT_HEADERFUNCTION, &HeaderFunction);
  if (!status.ok()) return OnTransferError(std::move(status));
  status = handle_.SetOption(CURLOPT_HEADERDATA, this);
  if (!status.ok()) return OnTransferError(std::move(status));
  status = handle_.SetOption(CURLOPT_WRITEFUNCTION, &WriteFunction);
  if (!status.ok()) return OnTransferError(std::move(status));
  status = handle_.SetOption(CURLOPT_WRITEDATA, this);
  if (!status.ok()) return OnTransferError(std::move(status));
  handle_.FlushDebug(__func__);

  if (!curl_closed_ && paused_) {
    paused_ = false;
    status = handle_.EasyPause(CURLPAUSE_RECV_CONT);
    TRACE_STATE() << ", status=" << status;
    if (!status.ok()) return OnTransferError(std::move(status));
  }

  if (avail_.empty()) {
    // Once we have received the status and all the headers we have read
    // enough to satisfy calls to any of RestResponse's methods, and we can
    // stop reading until we have a user buffer to fill with the body.
    status = PerformWorkUntil(
        [this] { return curl_closed_ || paused_ || all_headers_received_; });
  } else {
    status = PerformWorkUntil(
        [this] { return curl_closed_ || paused_ || avail_.empty(); });
  }

  TRACE_STATE() << ", status=" << status;
  if (!status.ok()) return OnTransferError(std::move(status));

  bytes_read = output.size() - avail_.size();
  if (curl_closed_) {
    OnTransferDone();
    return bytes_read;
  }
  TRACE_STATE() << ", http code=" << http_code_;
  return bytes_read;
}

void CurlImpl::CleanupHandles() {
  if (!multi_ != !handle_.handle_) {
    GCP_LOG(FATAL) << "handles are inconsistent, multi_=" << multi_.get()
                   << ", handle_.handle_=" << handle_.handle_.get();
  }

  // Remove the handle from the CURLM* interface and wait for the response.
  if (in_multi_) {
    (void)curl_multi_remove_handle(multi_.get(), handle_.handle_.get());
    in_multi_ = false;
    TRACE_STATE();
  }

  if (curl_closed_ || !multi_) return;

  if (paused_) {
    paused_ = false;
    (void)handle_.EasyPause(CURLPAUSE_RECV_CONT);
    TRACE_STATE();
  }
}

StatusOr<int> CurlImpl::PerformWork() {
  TRACE_STATE();
  if (!in_multi_) return 0;
  // Block while there is work to do, apparently newer versions of libcurl do
  // not need this loop and curl_multi_perform() blocks until there is no more
  // work, but is it pretty harmless to keep here.
  int running_handles = 0;
  CURLMcode multi_perform_result;
  do {
    multi_perform_result = curl_multi_perform(multi_.get(), &running_handles);
  } while (multi_perform_result == CURLM_CALL_MULTI_PERFORM);

  if (multi_perform_result != CURLM_OK) {
    auto status = AsStatus(multi_perform_result, __func__);
    TRACE_STATE() << ", status=" << status;
    return status;
  }

  if (running_handles == 0) {
    // The only way we get here is if the handle "completed", and therefore the
    // transfer either failed or was successful. Pull all the messages out of
    // the info queue until we get the message about our handle.
    int remaining;
    while (auto* msg = curl_multi_info_read(multi_.get(), &remaining)) {
      if (msg->easy_handle != handle_.handle_.get()) {
        // Return an error if this is the wrong handle. This should never
        // happen. If it does, we are using the libcurl API incorrectly. But it
        // is better to give a meaningful error message in this case.
        std::ostringstream os;
        os << __func__ << " unknown handle returned by curl_multi_info_read()"
           << ", msg.msg=[" << msg->msg << "]"
           << ", result=[" << msg->data.result
           << "]=" << curl_easy_strerror(msg->data.result);
        return internal::UnknownError(std::move(os).str());
      }

      auto multi_info_read_result = msg->data.result;
      TRACE_STATE() << ", status="
                    << CurlHandle::AsStatus(multi_info_read_result, __func__)
                    << ", remaining=" << remaining
                    << ", running_handles=" << running_handles;
      // Whatever the status is, the transfer is done, we need to remove it
      // from the CURLM* interface.
      curl_closed_ = true;
      CURLMcode multi_remove_result = CURLM_OK;
      if (in_multi_) {
        // In the extremely unlikely case that removing the handle from CURLM*
        // was an error, return that as a status.
        multi_remove_result =
            curl_multi_remove_handle(multi_.get(), handle_.handle_.get());
        in_multi_ = false;
      }

      TRACE_STATE() << ", status="
                    << CurlHandle::AsStatus(multi_info_read_result, __func__)
                    << ", remaining=" << remaining
                    << ", running_handles=" << running_handles
                    << ", multi_remove_status="
                    << AsStatus(multi_remove_result, __func__);

      // Ignore errors when closing the handle. They are expected because
      // libcurl may have received a block of data, but the WriteCallback()
      // (see above) tells libcurl that it cannot receive more data.
      if (closing_) continue;
      if (multi_info_read_result != CURLE_OK) {
        return CurlHandle::AsStatus(multi_info_read_result, __func__);
      }
      if (multi_remove_result != CURLM_OK) {
        return AsStatus(multi_remove_result, __func__);
      }
    }
  }
  TRACE_STATE() << ", running_handles=" << running_handles;
  return running_handles;
}

Status CurlImpl::PerformWorkUntil(absl::FunctionRef<bool()> predicate) {
  TRACE_STATE() << ", begin";
  int repeats = 0;
  while (!predicate()) {
    handle_.FlushDebug(__func__);
    TRACE_STATE() << ", repeats=" << repeats;
    auto running_handles = PerformWork();
    if (!running_handles.ok()) return std::move(running_handles).status();

    // Only wait if there are CURL handles with pending work *and* the
    // predicate is not satisfied. Note that if the predicate is ill-defined
    // it might continue to be unsatisfied even though the handles have
    // completed their work.
    if (*running_handles == 0 || predicate()) break;
    auto status = WaitForHandles(repeats);
    if (!status.ok()) return status;
  }
  return {};
}

Status CurlImpl::WaitForHandles(int& repeats) {
  int const timeout_ms = 1000;
  int numfds = 0;
#if CURL_AT_LEAST_VERSION(7, 66, 0)
  CURLMcode result =
      curl_multi_poll(multi_.get(), nullptr, 0, timeout_ms, &numfds);
  TRACE_STATE() << ", numfds=" << numfds << ", result=" << result
                << ", repeats=" << repeats;
  if (result != CURLM_OK) return AsStatus(result, __func__);
#else
  CURLMcode result =
      curl_multi_wait(multi_.get(), nullptr, 0, timeout_ms, &numfds);
  TRACE_STATE() << ", numfds=" << numfds << ", result=" << result
                << ", repeats=" << repeats;
  if (result != CURLM_OK) return AsStatus(result, __func__);
  // The documentation for curl_multi_wait() recommends sleeping if it returns
  // numfds == 0 more than once in a row :shrug:
  //    https://curl.haxx.se/libcurl/c/curl_multi_wait.html
  if (numfds == 0) {
    if (++repeats > 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
    }
  } else {
    repeats = 0;
  }
#endif
  return {};
}

Status CurlImpl::OnTransferError(Status status) {
  // When there is a transfer error the handle is suspect. It could be pointing
  // to an invalid host, a host that is slow and trickling data, or otherwise
  // be in a bad state. Release the handle, but do not return it to the pool.
  CleanupHandles();
  CurlHandle::DiscardFromPool(*factory_, std::move(handle_));

  // While the handle is suspect, there is probably nothing wrong with the
  // CURLM* handle. That just represents a local resource, such as data
  // structures for epoll(7) or select(2).
  factory_->CleanupMultiHandle(std::move(multi_), HandleDisposition::kKeep);

  return status;
}

void CurlImpl::OnTransferDone() {
  http_code_ = static_cast<HttpStatusCode>(handle_.GetResponseCode());
  TRACE_STATE() << ", done";

  // handle_ was removed from multi_ as part of the transfer completing
  // in PerformWork(). Release the handles back to the factory as soon as
  // possible, so they can be reused for any other requests.
  CurlHandle::ReturnToPool(*factory_, std::move(handle_));
  factory_->CleanupMultiHandle(std::move(multi_), HandleDisposition::kKeep);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

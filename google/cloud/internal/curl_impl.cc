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
#if 0
#include "google/cloud/internal/curl_impl.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/curl_options.h"
#include "google/cloud/internal/rest_options.h"
#include "google/cloud/internal/user_agent_prefix.h"
#include "google/cloud/log.h"
#include <nlohmann/json.hpp>
#include <numeric>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void CurlImpl::SetHeader(std::string const& header) {

}

void CurlImpl::SetHeader(std::pair<std::string, std::string> const& header) {

}

void CurlImpl::SetHeaders(RestRequest const& request) {

}

void CurlImpl::SetUrl(std::string const& endpoint, RestRequest const& request,
            RestRequest::HttpParameters const& additional_parameters) {

}

std::string CurlImpl::LastClientIpAddress() const {
  return {};
}

HttpStatusCode CurlImpl::status_code() const {
  return HttpStatusCode::kOk;
}


std::string CurlImpl::MakeEscapedString(std::string const& payload);

Status CurlImpl::MakeRequest(CurlImpl::HttpMethod method,
                   std::vector<absl::Span<char const>> payload = {});
StatusOr<std::size_t> CurlImpl::Read(absl::Span<char> buf);

#if 0
// Note that TRACE-level messages are disabled by default, even in
// `CMAKE_BUILD_TYPE=Debug` builds. The level of detail created by the
// `TRACE_STATE()` macro is only needed by the library developers when
// troubleshooting this class.
#define TRACE_STATE()                                                       \
  GCP_LOG(TRACE) << __func__ << "(), buffer_size_=" << buffer_size_         \
                 << ", buffer_offset_=" << buffer_offset_                   \
                 << ", spill_.max_size()=" << spill_.max_size()             \
                 << ", spill_offset_=" << spill_offset_                     \
                 << ", closing=" << closing_ << ", closed=" << curl_closed_ \
                 << ", paused=" << paused_ << ", in_multi=" << in_multi_

namespace {
char const* InitialQueryParameterSeparator(std::string const& url) {
  if (url.find('?') != std::string::npos) return "&";
  return "?";
}

std::string UserAgentSuffix() {
  // Pre-compute and cache the user agent string:
  static auto const* const kUserAgentSuffix = new auto([] {
    std::string agent = google::cloud::internal::UserAgentPrefix() + " ";
    agent += curl_version();
    return agent;
  }());
  return *kUserAgentSuffix;
}

#if 0
std::size_t TotalBytes(std::vector<absl::Span<char const>> const& s) {
  return std::accumulate(
      s.begin(), s.end(), std::size_t{0},
      [](std::size_t a, absl::Span<char const> const& b) { return a + b.size(); });
}
#endif

void PopFrontBytes(std::vector<absl::Span<char const>>& s, std::size_t count) {
  auto i = s.begin();
  for (; i != s.end() && i->size() <= count; ++i) {
    count -= i->size();
  }
  if (i == s.end()) {
    s.clear();
    return;
  }
  // In practice this is expected to be cheap, most vectors will contain 1
  // or 2 elements. And, if you are really lucky, your compiler turns this
  // into a memmove():
  //     https://godbolt.org/z/jw5VDd
  s.erase(s.begin(), i);
  if (count > 0 && !s.empty()) {
    s.front() = absl::Span<char const>(s.front().data() + count,
                                       s.front().size() - count);
  }
}

const char* HttpMethodAsChar(CurlImpl::HttpMethod method) {
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

std::string NormalizeEndpoint(std::string endpoint) {
  if (endpoint.empty()) return endpoint;
  if (endpoint.back() != '/') endpoint.push_back('/');
  return endpoint;
}

}  // namespace
#if 0
Status AsStatus(HttpStatusCode code, absl::Span<char> payload) {
  auto const status_code = MapHttpCodeToStatus(code);
  if (status_code == StatusCode::kOk) return Status{};

  // We try to parse the payload as JSON, which may allow us to provide a more
  // structured and useful error Status. If the payload fails to parse as JSON,
  // we simply attach the full error payload as the Status's message string.
  std::string payload_string(payload.data(), payload.size());
  auto json = nlohmann::json::parse(payload_string, nullptr, false);
  if (json.is_discarded()) return Status(status_code, payload_string);

  // We expect JSON that looks like the following:
  //   {
  //     "error": {
  //       "message": "..."
  //       ...
  //       "details": [
  //         ...
  //       ]
  //     }
  //   }
  // See  https://cloud.google.com/apis/design/errors#http_mapping
  auto error = json.value("error", nlohmann::json::object());
  auto message = error.value("message", payload_string);
  auto details = error.value("details", nlohmann::json::object());
  auto error_info = MakeErrorInfo(details);
  return Status(status_code, std::move(message), std::move(error_info));
}
#endif

class WriteVector {
 public:
  explicit WriteVector(std::vector<absl::Span<char const>> w)
      : writev_(std::move(w)) {}

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
  std::vector<absl::Span<char const>> writev_;
};

#if 0
CurlImpl::WriteVector::WriteVector(std::vector<absl::Span<char const>> w)
      : writev_(std::move(w)) {}

bool CurlImpl::WriteVector::empty() const { return writev_.empty(); }

std::size_t CurlImpl::WriteVector::OnRead(char* ptr, std::size_t size, std::size_t nitems) {
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
#endif

extern "C" std::size_t CurlRequestWrite(char* ptr, size_t size, size_t nmemb,
                                        void* userdata) {
  auto* request = reinterpret_cast<CurlImpl*>(userdata);
  return request->WriteCallback(ptr, size, nmemb);
}

extern "C" std::size_t CurlRequestHeader(char* contents, std::size_t size,
                                         std::size_t nitems, void* userdata) {
  auto* request = reinterpret_cast<CurlImpl*>(userdata);
  return request->HeaderCallback(contents, size, nitems);
}

#if 0
extern "C" size_t CurlRequestOnWriteData(char* ptr, size_t size, size_t nmemb,
                                         void* userdata) {
  auto* request = reinterpret_cast<CurlImpl*>(userdata);
  return request->OnWriteData(ptr, size, nmemb);
}

extern "C" size_t CurlRequestOnHeaderData(char* contents, size_t size,
                                          size_t nitems, void* userdata) {
  auto* request = reinterpret_cast<CurlImpl*>(userdata);
  return request->OnHeaderData(contents, size, nitems);
}
#endif

extern "C" std::size_t CurlRequestOnReadData(char* ptr, std::size_t size,
                                             std::size_t nitems,
                                             void* userdata) {
  auto* v = reinterpret_cast<WriteVector*>(userdata);
  return v->OnRead(ptr, size, nitems);
}

void CurlImpl::ApplyOptions(Options const& options) {
  logging_enabled_ = google::cloud::internal::Contains(
      options.get<TracingComponentsOption>(), "http");
  socket_options_.recv_buffer_size_ =
      options.get<MaximumCurlSocketRecvSizeOption>();
  socket_options_.send_buffer_size_ =
      options.get<MaximumCurlSocketSendSizeOption>();
  auto agents = options.get<UserAgentProductsOption>();
  agents.push_back(user_agent_);
  user_agent_ = absl::StrCat(absl::StrJoin(agents, " "), UserAgentSuffix());
  http_version_ = std::move(options.get<HttpVersionOption>());
  transfer_stall_timeout_ = options.get<TransferStallTimeoutOption>();
  download_stall_timeout_ = options.get<DownloadStallTimeoutOption>();
}

CurlImpl::CurlImpl(CurlHandle handle,
                   std::shared_ptr<CurlHandleFactory> factory, Options options)
    : handle_(std::move(handle)),
      factory_(std::move(factory)),
      request_headers_(nullptr, &curl_slist_free_all),
      multi_(factory_->CreateMultiHandle()),
      logging_enabled_(false),
      transfer_stall_timeout_(0),
      download_stall_timeout_(0),
      closing_(false),
      curl_closed_(false),
      in_multi_(false),
      paused_(false),
      buffer_(nullptr),
      buffer_size_(0),
      buffer_offset_(0),
      spill_offset_(0),
      options_(std::move(options)) {
  CurlInitializeOnce(options_);
  ApplyOptions(options_);
}

CurlImpl::~CurlImpl() {
  TRACE_STATE() << "\n";
  if (buffer_ != nullptr) {
    TRACE_STATE() << ", buffer_: !nullptr"
                  << "\n";
  } else {
    TRACE_STATE() << ", buffer_: nullptr"
                  << "\n";
  }
  CleanupHandles();

  if (!curl_closed_) {
    // Set the the closing_ flag to trigger a return 0 from the next read
    // callback, see the comments in the header file for more details.
    closing_ = true;
    TRACE_STATE() << "\n";
    // Ignore errors. Except in some really unfortunate cases [*] we are closing
    // the download early. That is done [**] by having the write callback return
    // 0, which always results in libcurl returning `CURLE_WRITE_ERROR`.
    //
    // [*]: the only other case would be the case where a download completes
    //   and the handle is paused because just the right number of bytes
    //   arrived to satisfy the last `Read()` request. In that case ignoring the
    //   errors seems sensible too, the download completed, what is the problem?
    // [**]: this is the recommended practice to shutdown a download early. See
    //   the comments in the header file and elsewhere in this file.
    (void)handle_.EasyPerform();
    curl_closed_ = true;
    TRACE_STATE() << "\n";
    OnTransferDone();
  }

  if (factory_) {
    factory_->CleanupHandle(std::move(handle_));
    factory_->CleanupMultiHandle(std::move(multi_));
  }
}

std::size_t CurlImpl::WriteCallback(void* ptr, std::size_t size,
                                    std::size_t nmemb) {
  handle_.FlushDebug(__func__);
  TRACE_STATE() << ", begin"
                << ", n=" << size * nmemb << "\n";
  // This transfer is closing, just return zero, that will make libcurl finish
  // any pending work, and will return the handle_ pointer from
  // curl_multi_info_read() in PerformWork(). That is the point where
  // `curl_closed_` is set.
  if (closing_) {
    TRACE_STATE() << " closing << \n";
    return 0;
  }

  // If buffer_size_ is 0 then this is the initial call to make the request, and
  // we need to stash the received bytes into the spill buffer so that we can
  // make the response code and headers available without requiring the user to
  // read the payload of the response. Any payload bytes sequestered in the
  // spill buffer will be the first returned to the user on attempts to read
  // the payload. Only after the spill buffer has been emptied will we read more
  // from libcurl.
  if (buffer_size_ == 0) {
    if (spill_offset_ >= spill_.max_size()) {
      TRACE_STATE()
          << " spill_offset_ >= spill_.max_size() *** PAUSING HANDLE ***\n";
      paused_ = true;
      return CURL_WRITEFUNC_PAUSE;
    }

    std::size_t free = spill_.max_size() - spill_offset_;
    if (free == 0) {
      TRACE_STATE() << " (spill_.max_size() - spill_offset_) == 0 *** PAUSING "
                       "HANDLE ***\n";
      paused_ = true;
      return CURL_WRITEFUNC_PAUSE;
    }

    // Copy the full contents of `ptr` into the spill buffer.
    if (size * nmemb < free) {
      std::memcpy(spill_.data() + spill_offset_, ptr, size * nmemb);
      spill_offset_ += size * nmemb;
      TRACE_STATE() << ", copy full into spill"
                    << ", n=" << size * nmemb << "\n";
      return size * nmemb;
    }

    TRACE_STATE()
        << " can't read into the spill buffer *** PAUSING HANDLE ***\n";
    return CURL_WRITEFUNC_PAUSE;
  }

  // For a given request, the second and subsequent calls to Read should provide
  // a non-zero buffer_size_.
  if (buffer_offset_ >= buffer_size_) {
    TRACE_STATE()
        << " buffer_offset_ >= buffer_size_ *** PAUSING HANDLE *** << \n";
    paused_ = true;
    return CURL_WRITEFUNC_PAUSE;
  }

  // Use the spill buffer first, if there is any...
  DrainSpillBuffer();
  std::size_t free = buffer_size_ - buffer_offset_;
  if (free == 0) {
    TRACE_STATE()
        << " (buffer_size_ - buffer_offset_) == 0 *** PAUSING HANDLE *** \n";
    paused_ = true;
    return CURL_WRITEFUNC_PAUSE;
  }
  TRACE_STATE() << ", post drain"
                << ", n=" << size * nmemb << ", free=" << free << "\n";

  // Copy the full contents of `ptr` into the application buffer.
  if (size * nmemb < free) {
    std::memcpy(buffer_ + buffer_offset_, ptr, size * nmemb);
    buffer_offset_ += size * nmemb;
    TRACE_STATE() << ", copy full"
                  << ", n=" << size * nmemb << "\n";
    return size * nmemb;
  }
  // Copy as much as possible from `ptr` into the application buffer.
  std::memcpy(buffer_ + buffer_offset_, ptr, free);
  buffer_offset_ += free;
  spill_offset_ = size * nmemb - free;
  // The rest goes into the spill buffer.
  std::memcpy(spill_.data(), static_cast<char*>(ptr) + free, spill_offset_);
  TRACE_STATE() << ", copy as much"
                << ", n=" << size * nmemb << ", free=" << free << "\n";
  return size * nmemb;
}

std::size_t CurlImpl::HeaderCallback(char* contents, std::size_t size,
                                     std::size_t nitems) {
  return CurlAppendHeaderData(received_headers_, contents, size * nitems);
}

void CurlImpl::SetHeader(std::string const& header) {
  auto* new_headers = curl_slist_append(request_headers_.get(), header.c_str());
  (void)request_headers_.release();
  request_headers_.reset(new_headers);
}

void CurlImpl::SetHeader(const std::pair<std::string, std::string>& header) {
  SetHeader(absl::StrCat(header.first, ": ", header.second));
}

void CurlImpl::SetHeaders(RestRequest const& request) {
  // TODO(sdhart): this seems expensive to have to call curl_slist_append per
  //  header. Look to see if libcurl has a mechanism to add multiple headers
  //  at once.
  for (auto const& header : request.headers()) {
    std::string formatted_header =
        absl::StrCat(header.first, ": ", absl::StrJoin(header.second, ","));
    auto* new_headers =
        curl_slist_append(request_headers_.get(), formatted_header.c_str());
    (void)request_headers_.release();
    request_headers_.reset(new_headers);
  }
}

void CurlImpl::SetUrl(
    std::string const& endpoint, RestRequest const& request,
    RestRequest::HttpParameters const& additional_parameters) {
  if (request.path().empty() && additional_parameters.empty()) {
    url_ = endpoint;
    return;
  }
  url_ = absl::StrCat(NormalizeEndpoint(endpoint), request.path());
  const char* query_parameter_separator = InitialQueryParameterSeparator(url_);
  auto append_params = [&](RestRequest::HttpParameters const& parameters) {
    for (auto const& param : parameters) {
      absl::StrAppend(
          &url_, absl::StrCat(query_parameter_separator,
                              handle_.MakeEscapedString(param.first).get(), "=",
                              handle_.MakeEscapedString(param.second).get()));
      query_parameter_separator = "&";
    }
  };

  append_params(request.parameters());
  append_params(additional_parameters);
}

std::string CurlImpl::LastClientIpAddress() const {
  return factory_->LastClientIpAddress();
}

HttpStatusCode CurlImpl::status_code() const {
  return static_cast<HttpStatusCode>(http_code_);
}

std::multimap<std::string, std::string> const& CurlImpl::headers() const {
  return received_headers_;
}

std::string const& CurlImpl::url() const { return url_; }

std::string CurlImpl::MakeEscapedString(std::string const& payload) {
  return handle_.MakeEscapedString(payload).get();
}

void CurlImpl::OnTransferDone() {
  // Retrieve the response code for a closed stream. Note the use of
  // `.value()`, this is equivalent to: assert(http_code.ok());
  // The only way the previous call can fail indicates a bug in our code (or
  // corrupted memory), the documentation for CURLINFO_RESPONSE_CODE:
  //   https://curl.haxx.se/libcurl/c/CURLINFO_RESPONSE_CODE.html
  // says:
  //   Returns CURLE_OK if the option is supported, and CURLE_UNKNOWN_OPTION
  //   if not.
  // if the option is not supported then we cannot use HTTP at all in libcurl
  // and the whole library would be unusable.
  http_code_ = handle_.GetResponseCode().value();

  // Capture the peer (the HTTP server), used for troubleshooting.
  received_headers_.emplace(":curl-peer", handle_.GetPeer());
  TRACE_STATE() << "\n";

  // Release the handles back to the factory as soon as possible, so they can be
  // reused for any other requests.
  if (factory_) {
    factory_->CleanupHandle(std::move(handle_));
    factory_->CleanupMultiHandle(std::move(multi_));
  }
}

Status CurlImpl::OnTransferError(Status status) {
  // When there is a transfer error the handle is suspect. It could be pointing
  // to an invalid host, a host that is slow and trickling data, or otherwise in
  // a bad state. Release the handle, but do not return it to the pool.
  CleanupHandles();
  auto handle = std::move(handle_);
  if (factory_) {
    // While the handle is suspect, there is probably nothing wrong with the
    // CURLM* handle, that just represents a local resource, such as data
    // structures for `epoll(7)` or `select(2)`
    factory_->CleanupMultiHandle(std::move(multi_));
  }
  return status;
}

void CurlImpl::DrainSpillBuffer() {
  handle_.FlushDebug(__func__);
  TRACE_STATE() << ", begin\n";
  std::size_t free = buffer_size_ - buffer_offset_;
  auto copy_count = (std::min)(free, spill_offset_);
  std::copy(spill_.data(), spill_.data() + copy_count,
            buffer_ + buffer_offset_);
  buffer_offset_ += copy_count;
  std::memmove(spill_.data(), spill_.data() + copy_count,
               spill_.size() - copy_count);
  spill_offset_ -= copy_count;
  TRACE_STATE() << ", end\n";
}

Status CurlImpl::Wait(absl::FunctionRef<bool()> predicate) {
  TRACE_STATE() << ", begin\n";
  int repeats = 0;
  // We can assert that the current thread is the leader, because the
  // predicate is satisfied, and the condition variable exited. Therefore,
  // this thread must run the I/O event loop.
  while (!predicate()) {
    handle_.FlushDebug(__func__);
    TRACE_STATE() << ", repeats=" << repeats << "\n";
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
  return Status();
}

StatusOr<int> CurlImpl::PerformWork() {
  TRACE_STATE() << "\n";
  if (!in_multi_) {
    return 0;
  }

  // Block while there is work to do, apparently newer versions of libcurl do
  // not need this loop and curl_multi_perform() blocks until there is no more
  // work, but is it pretty harmless to keep here.
  int running_handles = 0;
  CURLMcode result;
  do {
    result = curl_multi_perform(multi_.get(), &running_handles);
  } while (result == CURLM_CALL_MULTI_PERFORM);

  // Throw an exception if the result is unexpected, otherwise return.
  auto status = AsStatus(result, __func__);
  if (!status.ok()) {
    TRACE_STATE() << ", status=" << status << "\n";
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
        // happen, if it does we are using the libcurl API incorrectly. But it
        // is better to give a meaningful error message in this case.
        std::ostringstream os;
        os << __func__ << " unknown handle returned by curl_multi_info_read()"
           << ", msg.msg=[" << msg->msg << "]"
           << ", result=[" << msg->data.result
           << "]=" << curl_easy_strerror(msg->data.result);
        return Status(StatusCode::kUnknown, std::move(os).str());
      }
      status = CurlHandle::AsStatus(msg->data.result, __func__);
      TRACE_STATE() << ", status=" << status << ", remaining=" << remaining
                    << ", running_handles=" << running_handles << "\n";
      // Whatever the status is, the transfer is done, we need to remove it
      // from the CURLM* interface.
      curl_closed_ = true;
      Status multi_remove_status;
      if (in_multi_) {
        // In the extremely unlikely case that removing the handle from CURLM*
        // was an error, return that as a status.
        multi_remove_status = AsStatus(
            curl_multi_remove_handle(multi_.get(), handle_.handle_.get()),
            __func__);
        in_multi_ = false;
      }

      TRACE_STATE() << ", status=" << status << ", remaining=" << remaining
                    << ", running_handles=" << running_handles
                    << ", multi_remove_status=" << multi_remove_status << "\n";

      // Ignore errors when closing the handle. They are expected because
      // libcurl may have received a block of data, but the WriteCallback()
      // (see above) tells libcurl that it cannot receive more data.
      if (closing_) {
        continue;
      }
      if (!status.ok()) {
        return status;
      }
      if (!multi_remove_status.ok()) {
        return multi_remove_status;
      }
    }
  }
  TRACE_STATE() << ", running_handles=" << running_handles << "\n";
  return running_handles;
}

Status CurlImpl::WaitForHandles(int& repeats) {
  int const timeout_ms = 1;
  std::chrono::milliseconds const timeout(timeout_ms);
  int numfds = 0;
  CURLMcode result =
      curl_multi_wait(multi_.get(), nullptr, 0, timeout_ms, &numfds);
  TRACE_STATE() << ", numfds=" << numfds << ", result=" << result
                << ", repeats=" << repeats << "\n";
  Status status = AsStatus(result, __func__);
  if (!status.ok()) {
    return status;
  }
  // The documentation for curl_multi_wait() recommends sleeping if it returns
  // numfds == 0 more than once in a row :shrug:
  //    https://curl.haxx.se/libcurl/c/curl_multi_wait.html
  if (numfds == 0) {
    if (++repeats > 1) {
      std::this_thread::sleep_for(timeout);
    }
  } else {
    repeats = 0;
  }
  return status;
}

Status CurlImpl::AsStatus(CURLMcode result, char const* where) {
  if (result == CURLM_OK) {
    return Status();
  }
  std::ostringstream os;
  os << where << "(): unexpected error code in curl_multi_*, [" << result
     << "]=" << curl_multi_strerror(result);
  return Status(StatusCode::kUnknown, std::move(os).str());
}

StatusOr<std::size_t> CurlImpl::Read() { return ReadImpl({nullptr, 0}); }

StatusOr<std::size_t> CurlImpl::Read(absl::Span<char> buf) {
  if (buf.empty())
    return Status(StatusCode::kInvalidArgument,
                  "Read buffer size must be non-zero", {});
  return ReadImpl(std::move(buf));
}

StatusOr<std::size_t> CurlImpl::ReadImpl(absl::Span<char> buf) {
  TRACE_STATE() << "begin\n";
  buffer_ = buf.data();
  buffer_offset_ = 0;
  buffer_size_ = buf.size();
  // Before calling `Wait()` copy any data from the spill buffer into the
  // application buffer. It is possible that `Wait()` will never call
  // `WriteCallback()`, for example, because the Read() or Peek() closed the
  // connection, but if there is any data left in the spill buffer we need
  // to return it.
  DrainSpillBuffer();
  if (curl_closed_) {
    return buffer_offset_;
  }

  handle_.SetOption(CURLOPT_WRITEFUNCTION, &CurlRequestWrite);
  handle_.SetOption(CURLOPT_WRITEDATA, this);
  handle_.SetOption(CURLOPT_HEADERFUNCTION, &CurlRequestHeader);
  handle_.SetOption(CURLOPT_HEADERDATA, this);
  handle_.FlushDebug(__func__);

  if (!curl_closed_ && paused_) {
    paused_ = false;
    auto status = handle_.EasyPause(CURLPAUSE_RECV_CONT);
    TRACE_STATE() << ", status=" << status << "\n";
    if (!status.ok()) return status;
  }

  Status status;
  if (buffer_size_ == 0) {
    status = Wait([this] {
      return curl_closed_ || paused_ || spill_offset_ >= spill_.max_size();
    });
  } else {
    status = Wait([this] {
      return curl_closed_ || paused_ || buffer_offset_ >= buffer_size_;
    });
  }
  TRACE_STATE() << ", status=" << status << "\n";
  if (!status.ok()) return OnTransferError(std::move(status));
  auto bytes_read = buffer_offset_;

  if (curl_closed_) {
    OnTransferDone();
    status = google::cloud::rest_internal::AsStatus(
        static_cast<HttpStatusCode>(http_code_), {});
    TRACE_STATE() << ", status=" << status << ", http code=" << http_code_
                  << "\n";
    if (!status.ok()) return status;
    return bytes_read;
  }
  TRACE_STATE() << ", http code=" << http_code_ << "\n";
  received_headers_.emplace(":curl-peer", handle_.GetPeer());
  return bytes_read;
}

void CurlImpl::CleanupHandles() {
  if (!multi_ != !handle_.handle_) {
    GCP_LOG(FATAL) << "handles are inconsistent, multi_=" << multi_.get()
                   << ", handle_.handle_=" << handle_.handle_.get();
  }
  if (curl_closed_ || !multi_) return;

  if (paused_) {
    paused_ = false;
    (void)handle_.EasyPause(CURLPAUSE_RECV_CONT);
    TRACE_STATE() << "\n";
  }

  // Now remove the handle from the CURLM* interface and wait for the response.
  if (in_multi_) {
    (void)curl_multi_remove_handle(multi_.get(), handle_.handle_.get());
    in_multi_ = false;
    TRACE_STATE() << "\n";
  }
}

// It looks like we need to implement this as a curl_download_request, always
// using a multi handle. Since we want RestResponse to be able to provide status
// code and headers, we must call "Read" for 0 bytes. Currently, a call to
// "Read" for 0 bytes is an error. This should cause all bytes returned to go
// into the spill buffer, but hopefully will get us the status code and headers.
// Then, if the user calls ExtractPayload the first Read will come from the
// spill buffer before we unpause any curl handles.
Status CurlImpl::MakeRequestImpl() {
  TRACE_STATE() << "url_ " << url_ << "\n";
  // We get better performance using a slightly larger buffer (128KiB) than the
  // default buffer size set by libcurl (16KiB)
  auto constexpr kDefaultBufferSize = 128 * 1024L;

  handle_.SetOption(CURLOPT_BUFFERSIZE, kDefaultBufferSize);
  handle_.SetOption(CURLOPT_URL, url_.c_str());
  handle_.SetOption(CURLOPT_HTTPHEADER, request_headers_.get());
  handle_.SetOption(CURLOPT_USERAGENT, user_agent_.c_str());
  handle_.EnableLogging(logging_enabled_);
  handle_.SetSocketCallback(socket_options_);
  handle_.SetOptionUnchecked(CURLOPT_HTTP_VERSION,
                             VersionToCurlCode(http_version_));
  handle_.SetOption(CURLOPT_NOSIGNAL, 1);
  // above this line are same between curl_request and curl_download_request

  // curl_download_request sets these additional
  handle_.SetOption(CURLOPT_NOPROGRESS, 1L);
  if (in_multi_) GCP_LOG(FATAL) << "in_multi_ should be false in `SetOptions`";
  auto error = curl_multi_add_handle(multi_.get(), handle_.handle_.get());
  if (error != CURLM_OK) {
    // This indicates that we are using the API incorrectly, the application
    // can not recover from these problems, raising an exception is the
    // "Right Thing"[tm] here.
    google::cloud::internal::ThrowStatus(AsStatus(error, __func__));
  }
  in_multi_ = true;

#if 0
  // curl_request sets these
  handle_.SetOption(CURLOPT_TCP_KEEPALIVE, 1L);
  handle_.SetOption(CURLOPT_WRITEFUNCTION, &CurlRequestOnWriteData);
  handle_.SetOption(CURLOPT_WRITEDATA, this);
  handle_.SetOption(CURLOPT_HEADERFUNCTION, &CurlRequestOnHeaderData);
  handle_.SetOption(CURLOPT_HEADERDATA, this);
#endif

#if 0
  // curl_request uses transfer_stall_timeout
  // curl_download_request uses download_stall_timeout
  // body of if is otherwise the same
  if (transfer_stall_timeout_.count() != 0) {
    // NOLINTNEXTLINE(google-runtime-int) - libcurl *requires* `long`
    auto const timeout = static_cast<long>(transfer_stall_timeout_.count());
    handle_.SetOption(CURLOPT_CONNECTTIMEOUT, timeout);
    // Timeout if the request sends or receives less than 1 byte/second (i.e.
    // effectively no bytes) for `transfer_stall_timeout_` seconds.
    handle_.SetOption(CURLOPT_LOW_SPEED_LIMIT, 1L);
    handle_.SetOption(CURLOPT_LOW_SPEED_TIME, timeout);
  }
#endif

#if 0
  // curl_request
  auto status = handle_.EasyPerform();
  if (!status.ok()) return OnError(std::move(status));

  if (logging_enabled_) handle_.FlushDebug(__func__);
  auto code = handle_.GetResponseCode();
  if (!code.ok()) return std::move(code).status();

  response_payload_.clear();
#endif

  // This call to Read should send the request, get the response, and thus make
  // available the status_code and headers. Any payload bytes should be put into
  // the spill buffer.
  auto read_status = Read();
  return read_status.status();
}

Status CurlImpl::MakeRequest(CurlImpl::HttpMethod method,
                             std::vector<absl::Span<char const>> payload) {
  using HttpMethod = CurlImpl::HttpMethod;
  handle_.SetOption(CURLOPT_CUSTOMREQUEST, HttpMethodAsChar(method));
  handle_.SetOption(CURLOPT_UPLOAD, 0L);

  if (method == HttpMethod::kGet) {
    if (download_stall_timeout_.count() != 0) {
      // NOLINTNEXTLINE(google-runtime-int) - libcurl *requires* `long`
      auto const timeout = static_cast<long>(download_stall_timeout_.count());
      handle_.SetOption(CURLOPT_CONNECTTIMEOUT, timeout);
      // Timeout if the request sends or receives less than 1 byte/second (i.e.
      // effectively no bytes) for `download_stall_timeout_` seconds.
      handle_.SetOption(CURLOPT_LOW_SPEED_LIMIT, 1L);
      handle_.SetOption(CURLOPT_LOW_SPEED_TIME, timeout);
    }
    return MakeRequestImpl();
  }

  if (transfer_stall_timeout_.count() != 0) {
    // NOLINTNEXTLINE(google-runtime-int) - libcurl *requires* `long`
    auto const timeout = static_cast<long>(transfer_stall_timeout_.count());
    handle_.SetOption(CURLOPT_CONNECTTIMEOUT, timeout);
    // Timeout if the request sends or receives less than 1 byte/second (i.e.
    // effectively no bytes) for `transfer_stall_timeout_` seconds.
    handle_.SetOption(CURLOPT_LOW_SPEED_LIMIT, 1L);
    handle_.SetOption(CURLOPT_LOW_SPEED_TIME, timeout);
  }

  if (method == HttpMethod::kDelete || payload.empty()) {
    return MakeRequestImpl();
  }

  // TODO(sdhart): add support for multi-Span POST
  if (method == HttpMethod::kPost) {
    if (payload.size() == 1) {
      handle_.SetOption(CURLOPT_POSTFIELDSIZE, payload[0].size());
      handle_.SetOption(CURLOPT_POSTFIELDS, payload[0].data());
      return MakeRequestImpl();
    }
    return Status{
        StatusCode::kInvalidArgument,
        absl::StrCat(method, " with more than one Span not supported"),
        {}};
  }

  if (method == HttpMethod::kPut || method == HttpMethod::kPatch) {
    WriteVector writev{std::move(payload)};
    handle_.SetOption(CURLOPT_READFUNCTION, &CurlRequestOnReadData);
    handle_.SetOption(CURLOPT_READDATA, &writev);
    handle_.SetOption(CURLOPT_UPLOAD, 1L);
    return MakeRequestImpl();
  }

  return Status{StatusCode::kInvalidArgument,
                absl::StrCat("Unknown method: ", method),
                {}};
}
#endif
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
#endif
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
#include "google/cloud/internal/rest_options.h"
#include "google/cloud/internal/user_agent_prefix.h"
#include "google/cloud/log.h"
#include "absl/strings/ascii.h"
#include <numeric>

// Note that TRACE-level messages are disabled by default, even in
// `CMAKE_BUILD_TYPE=Debug` builds. The level of detail created by the
// `TRACE_STATE()` macro is only needed by the library developers when
// troubleshooting this class.
#define TRACE_STATE()                                                       \
  GCP_LOG(TRACE) << __func__ << "(), buffer_.size()=" << buffer_.size()     \
                 << ", spill_.max_size()=" << spill_.max_size()             \
                 << ", spill_offset_=" << spill_offset_                     \
                 << ", closing=" << closing_ << ", closed=" << curl_closed_ \
                 << ", paused=" << paused_ << ", in_multi=" << in_multi_

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
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

char const* HttpMethodAsChar(CurlImpl::HttpMethod method) {
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

  std::size_t size() const {
    std::size_t total_size = 0;
    for (auto const& s : writev_) {
      total_size += s.size();
    }
    return total_size;
  }

 private:
  std::vector<absl::Span<char const>> writev_;
};

// This helper function assumes there is sufficient space available in
// destination.
template <typename T>
absl::Span<T> WriteToBuffer(absl::Span<T> destination,
                            absl::Span<T> const& source) {
  std::copy(source.data(), source.data() + source.size(), destination.data());
  return absl::Span<T>(destination.data() + source.size(),
                       destination.size() - source.size());
}

}  // namespace

extern "C" std::size_t RestCurlRequestWrite(char* ptr, size_t size,
                                            size_t nmemb, void* userdata) {
  auto* request = reinterpret_cast<CurlImpl*>(userdata);
  return request->WriteCallback(ptr, size, nmemb);
}

extern "C" std::size_t RestCurlRequestHeader(char* contents, std::size_t size,
                                             std::size_t nitems,
                                             void* userdata) {
  auto* request = reinterpret_cast<CurlImpl*>(userdata);
  return request->HeaderCallback(contents, size, nitems);
}

extern "C" std::size_t RestCurlRequestOnReadData(char* ptr, std::size_t size,
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
  ignored_http_error_codes_ = options.get<IgnoredHttpErrorCodes>();
}

CurlImpl::CurlImpl(CurlHandle handle,
                   std::shared_ptr<CurlHandleFactory> factory, Options options)
    : factory_(std::move(factory)),
      request_headers_(nullptr, &curl_slist_free_all),
      transfer_stall_timeout_(0),
      download_stall_timeout_(0),
      handle_(std::move(handle)),
      multi_(factory_->CreateMultiHandle()),
      buffer_({nullptr, 0}),
      options_(std::move(options)) {
  CurlInitializeOnce(options_);
  ApplyOptions(options_);
}

void CurlImpl::CleanupHandles() {
  if (!multi_ != !handle_.handle_) {
    GCP_LOG(FATAL) << "handles are inconsistent, multi_=" << multi_.get()
                   << ", handle_.handle_=" << handle_.handle_.get();
  }

  // Now remove the handle from the CURLM* interface and wait for the response.
  if (in_multi_) {
    (void)curl_multi_remove_handle(multi_.get(), handle_.handle_.get());
    in_multi_ = false;
    TRACE_STATE() << "\n";
  }

  if (curl_closed_ || !multi_) return;

  if (paused_) {
    paused_ = false;
    (void)handle_.EasyPause(CURLPAUSE_RECV_CONT);
    TRACE_STATE() << "\n";
  }
}

CurlImpl::~CurlImpl() {
  if (!curl_closed_) {
    // Set the closing_ flag to trigger a return 0 from the next read callback,
    // see the comments in the header file for more details.
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
  }

  CleanupHandles();

  if (factory_) {
    factory_->CleanupHandle(std::move(handle_));
    factory_->CleanupMultiHandle(std::move(multi_));
  }
}

std::size_t CurlImpl::WriteAllBytesToSpillBuffer(void* ptr, std::size_t size,
                                                 std::size_t nmemb) {
  // The size of the spill buffer is the same as the max size curl
  // will write in one invocation. libcurl should never give us more bytes than
  // we have set in CURLOPT_BUFFERSIZE, on a single write. However, libcurl can
  // give us fewer bytes which can result in multiple calls to this function on
  // the initial read.
  if (size * nmemb > spill_.max_size() - spill_offset_) {
    GCP_LOG(FATAL) << absl::StrCat(
                          "libcurl attempted to write ",
                          std::to_string(size * nmemb),
                          " bytes into spill buffer with remaining capacity ",
                          std::to_string(spill_.max_size() - spill_offset_))
                   << "\n";
  }
  // As this function can be called multiple times on the first Read, the spill
  // buffer may be written to several times before it is full.
  std::memcpy(spill_.data() + spill_offset_, ptr, size * nmemb);
  spill_offset_ += size * nmemb;
  TRACE_STATE() << ", copy full into spill"
                << ", n=" << size * nmemb << "\n";
  return size * nmemb;
}

std::size_t CurlImpl::WriteToUserBuffer(void* ptr, std::size_t size,
                                        std::size_t nmemb) {
  if (buffer_.empty()) {
    TRACE_STATE()
        << " buffer_offset_ >= buffer_size_ *** PAUSING HANDLE *** << \n";
    paused_ = true;
    return CURL_WRITEFUNC_PAUSE;
  }

  // TODO(#8059): Consider refactoring moving bytes around between the source
  // buffer, spill buffer, and user buffer by defining all such buffers in terms
  // of absl::Span<char>.
  // Use the spill buffer first, if there is any...
  (void)DrainSpillBuffer();
  std::size_t free = buffer_.size();
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
    buffer_ =
        WriteToBuffer(std::move(buffer_),
                      absl::Span<char>(static_cast<char*>(ptr), size * nmemb));

    TRACE_STATE() << ", copy full"
                  << ", n=" << size * nmemb << "\n";
    return size * nmemb;
  }

  // Copy as much as possible from `ptr` into the application buffer.
  buffer_ = WriteToBuffer(std::move(buffer_),
                          absl::Span<char>(static_cast<char*>(ptr), free));
  // The rest goes into the spill buffer.
  spill_offset_ = size * nmemb - free;
  std::memcpy(spill_.data(), static_cast<char*>(ptr) + free, spill_offset_);
  TRACE_STATE() << ", copy as much"
                << ", n=" << size * nmemb << ", free=" << free << "\n";
  return size * nmemb;
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

  // If headers have not been received and buffer_size_ is 0 then this is the
  // initial call to make the request, and we need to stash the received bytes
  // into the spill buffer so that we can make the response code and headers
  // available without requiring the user to read the payload of the response.
  // Any payload bytes sequestered in the spill buffer will be the first
  // returned to the user on attempts to read the payload. Only after the spill
  // buffer has been emptied will we read more from handle_.
  if (!all_headers_received_ && buffer_.empty()) {
    all_headers_received_ = true;
    http_code_ = handle_.GetResponseCode();
    return WriteAllBytesToSpillBuffer(ptr, size, nmemb);
  }
  return WriteToUserBuffer(ptr, size, nmemb);
}

// libcurl invokes the HEADERFUNCTION exactly once for each header line received
// and only a complete header line is passed to the function. Additionally, the
// status line and blank lines preceding and following the headers are passed
// to this function.
std::size_t CurlImpl::HeaderCallback(char* contents, std::size_t size,
                                     std::size_t nitems) {
  return CurlAppendHeaderData(received_headers_, contents, size * nitems);
}

void CurlImpl::SetHeader(std::string const& header) {
  // TODO(9200): Figure out where this empty authorization header is being
  // added and fix it.
  if (header == "authorization: ") {
    return;
  }
  if (header.empty()) return;
  auto* new_headers = curl_slist_append(request_headers_.get(), header.c_str());
  (void)request_headers_.release();
  request_headers_.reset(new_headers);
}

void CurlImpl::SetHeader(std::pair<std::string, std::string> const& header) {
  SetHeader(absl::StrCat(header.first, ": ", header.second));
}

void CurlImpl::SetHeaders(RestRequest const& request) {
  // TODO(#7957): this seems expensive to have to call curl_slist_append per
  //  header. Look to see if libcurl has a mechanism to add multiple headers
  //  at once.
  for (auto const& header : request.headers()) {
    std::string formatted_header =
        absl::StrCat(header.first, ": ", absl::StrJoin(header.second, ","));
    SetHeader(formatted_header);
  }
}

void CurlImpl::SetUrl(
    std::string const& endpoint, RestRequest const& request,
    RestRequest::HttpParameters const& additional_parameters) {
  if (request.path().empty() && additional_parameters.empty()) {
    url_ = endpoint;
    return;
  }

  if (absl::AsciiStrToLower(request.path()).substr(0, endpoint.size()) ==
      absl::AsciiStrToLower(endpoint)) {
    url_ = request.path();
  } else {
    url_ = absl::StrCat(NormalizeEndpoint(endpoint), request.path());
  }

  char const* query_parameter_separator = InitialQueryParameterSeparator(url_);
  auto append_params = [&](RestRequest::HttpParameters const& parameters) {
    for (auto const& param : parameters) {
      if (param.first == "userIp" && param.second.empty()) {
        absl::StrAppend(&url_, query_parameter_separator,
                        handle_.MakeEscapedString(param.first).get(), "=",
                        handle_.MakeEscapedString(LastClientIpAddress()).get());
      } else {
        absl::StrAppend(&url_, query_parameter_separator,
                        handle_.MakeEscapedString(param.first).get(), "=",
                        handle_.MakeEscapedString(param.second).get());
      }
      query_parameter_separator = "&";
    }
  };
  append_params(request.parameters());
  append_params(additional_parameters);
}

void CurlImpl::OnTransferDone() {
  http_code_ = handle_.GetResponseCode();
  // Capture the peer (the HTTP server), used for troubleshooting.
  received_headers_.emplace(":curl-peer", handle_.GetPeer());
  TRACE_STATE() << "\n";

  // handle_ was removed from multi_ as part of the transfer completing in
  // PerformWork.
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
  // While the handle is suspect, there is probably nothing wrong with the
  // CURLM* handle, that just represents a local resource, such as data
  // structures for `epoll(7)` or `select(2)`
  if (factory_) factory_->CleanupMultiHandle(std::move(multi_));
  return status;
}

std::size_t CurlImpl::DrainSpillBuffer() {
  handle_.FlushDebug(__func__);
  std::size_t free = buffer_.size();
  auto copy_count = (std::min)(free, spill_offset_);
  if (copy_count > 0) {
    TRACE_STATE() << ", drain n=" << copy_count << " from spill\n";
    buffer_ =
        WriteToBuffer(buffer_, absl::Span<char>(spill_.data(), copy_count));
    // TODO(#7958): Consider making the spill buffer a circular buffer and then
    // we could remove this memmove step.
    std::memmove(spill_.data(), spill_.data() + copy_count,
                 spill_.size() - copy_count);
    spill_offset_ -= copy_count;
  }
  return copy_count;
}

Status CurlImpl::PerformWorkUntil(absl::FunctionRef<bool()> predicate) {
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
  return {};
}

StatusOr<int> CurlImpl::PerformWork() {
  TRACE_STATE() << "\n";
  if (!in_multi_) return 0;
  // Block while there is work to do, apparently newer versions of libcurl do
  // not need this loop and curl_multi_perform() blocks until there is no more
  // work, but is it pretty harmless to keep here.
  int running_handles = 0;
  CURLMcode result;
  do {
    result = curl_multi_perform(multi_.get(), &running_handles);
  } while (result == CURLM_CALL_MULTI_PERFORM);

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
      if (closing_) continue;
      if (!status.ok()) return status;
      if (!multi_remove_status.ok()) return multi_remove_status;
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
  if (!status.ok()) return status;
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
  if (result == CURLM_OK) return Status();
  std::ostringstream os;
  os << where << "(): unexpected error code in curl_multi_*, [" << result
     << "]=" << curl_multi_strerror(result);
  return Status(StatusCode::kUnknown, std::move(os).str());
}

StatusOr<std::size_t> CurlImpl::Read(absl::Span<char> output) {
  if (output.empty())
    return Status(StatusCode::kInvalidArgument,
                  "Read output size must be non-zero", {});
  return ReadImpl(std::move(output));
}

StatusOr<std::size_t> CurlImpl::ReadImpl(absl::Span<char> output) {
  TRACE_STATE() << "begin\n";
  buffer_ = output;
  // Before calling `Wait()` copy any data from the spill buffer into the
  // application buffer. It is possible that `Wait()` will never call
  // `WriteCallback()`, for example, because the Read() or Peek() closed the
  // connection, but if there is any data left in the spill buffer we need
  // to return it.
  auto bytes_read = DrainSpillBuffer();
  if (curl_closed_) return bytes_read;

  Status status;
  status = handle_.SetOption(CURLOPT_WRITEFUNCTION, &RestCurlRequestWrite);
  if (!status.ok()) return OnTransferError(std::move(status));
  status = handle_.SetOption(CURLOPT_WRITEDATA, this);
  if (!status.ok()) return OnTransferError(std::move(status));
  status = handle_.SetOption(CURLOPT_HEADERFUNCTION, &RestCurlRequestHeader);
  if (!status.ok()) return OnTransferError(std::move(status));
  status = handle_.SetOption(CURLOPT_HEADERDATA, this);
  if (!status.ok()) return OnTransferError(std::move(status));
  handle_.FlushDebug(__func__);

  if (!curl_closed_ && paused_) {
    paused_ = false;
    status = handle_.EasyPause(CURLPAUSE_RECV_CONT);
    TRACE_STATE() << ", status=" << status << "\n";
    if (!status.ok()) return OnTransferError(std::move(status));
  }

  if (buffer_.empty()) {
    // Once we have received the status and all the headers, we have read
    // enough to satisfy calls to any of RestResponse's methods, and we can
    // stop reading until we have a user buffer to write the payload.
    status = PerformWorkUntil(
        [this] { return curl_closed_ || paused_ || all_headers_received_; });
  } else {
    status = PerformWorkUntil(
        [this] { return curl_closed_ || paused_ || buffer_.empty(); });
  }

  TRACE_STATE() << ", status=" << status << "\n";
  if (!status.ok()) return OnTransferError(std::move(status));
  bytes_read = output.size() - buffer_.size();

  if (curl_closed_) {
    OnTransferDone();
    status = google::cloud::rest_internal::AsStatus(
        static_cast<HttpStatusCode>(http_code_), {});
    TRACE_STATE() << ", status=" << status << ", http code=" << http_code_
                  << "\n";

    if (status.ok() ||
        internal::Contains(ignored_http_error_codes_, http_code_)) {
      return bytes_read;
    }
    return status;
  }
  TRACE_STATE() << ", http code=" << http_code_ << "\n";
  received_headers_.emplace(":curl-peer", handle_.GetPeer());
  return bytes_read;
}

Status CurlImpl::MakeRequestImpl() {
  TRACE_STATE() << "url_ " << url_ << "\n";
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

  // This indicates that we are using the API incorrectly, the application
  // can not recover from these problems, terminating is the "Right Thing"[tm]
  // here.
  if (error != CURLM_OK) {
    GCP_LOG(FATAL) << AsStatus(error, __func__) << "\n";
  }

  in_multi_ = true;

  // This call to Read should send the request, get the response, and thus make
  // available the status_code and headers. Any payload bytes should be put into
  // the spill buffer which makes them available for subsequent calls to Read
  // after the payload has been extracted from the response.
  return ReadImpl({nullptr, 0}).status();
}

Status CurlImpl::MakeRequest(CurlImpl::HttpMethod method,
                             std::vector<absl::Span<char const>> payload) {
  using HttpMethod = CurlImpl::HttpMethod;
  Status status;
  status = handle_.SetOption(CURLOPT_CUSTOMREQUEST, HttpMethodAsChar(method));
  if (!status.ok()) return OnTransferError(std::move(status));
  status = handle_.SetOption(CURLOPT_UPLOAD, 0L);
  if (!status.ok()) return OnTransferError(std::move(status));
  status =
      handle_.SetOption(CURLOPT_FOLLOWLOCATION,
                        options_.get<CurlFollowLocationOption>() ? 1L : 0L);
  if (!status.ok()) return OnTransferError(std::move(status));

  if (method == HttpMethod::kGet) {
    status = handle_.SetOption(CURLOPT_NOPROGRESS, 1L);
    if (!status.ok()) return OnTransferError(std::move(status));
    if (download_stall_timeout_.count() != 0) {
      // NOLINTNEXTLINE(google-runtime-int) - libcurl *requires* `long`
      auto const timeout = static_cast<long>(download_stall_timeout_.count());
      status = handle_.SetOption(CURLOPT_CONNECTTIMEOUT, timeout);
      if (!status.ok()) return OnTransferError(std::move(status));
      // Timeout if the request sends or receives less than 1 byte/second (i.e.
      // effectively no bytes) for `download_stall_timeout_` seconds.
      status = handle_.SetOption(CURLOPT_LOW_SPEED_LIMIT, 1L);
      if (!status.ok()) return OnTransferError(std::move(status));
      status = handle_.SetOption(CURLOPT_LOW_SPEED_TIME, timeout);
      if (!status.ok()) return OnTransferError(std::move(status));
    }
    return MakeRequestImpl();
  }

  if (transfer_stall_timeout_.count() != 0) {
    // NOLINTNEXTLINE(google-runtime-int) - libcurl *requires* `long`
    auto const timeout = static_cast<long>(transfer_stall_timeout_.count());
    status = handle_.SetOption(CURLOPT_CONNECTTIMEOUT, timeout);
    if (!status.ok()) return OnTransferError(std::move(status));
    // Timeout if the request sends or receives less than 1 byte/second (i.e.
    // effectively no bytes) for `transfer_stall_timeout_` seconds.
    status = handle_.SetOption(CURLOPT_LOW_SPEED_LIMIT, 1L);
    if (!status.ok()) return OnTransferError(std::move(status));
    status = handle_.SetOption(CURLOPT_LOW_SPEED_TIME, timeout);
    if (!status.ok()) return OnTransferError(std::move(status));
  }

  if (method == HttpMethod::kDelete || payload.empty())
    return MakeRequestImpl();

  if (method == HttpMethod::kPost) {
    WriteVector writev{std::move(payload)};
    status = handle_.SetOption(CURLOPT_POSTFIELDS, nullptr);
    if (!status.ok()) return OnTransferError(std::move(status));
    status = handle_.SetOption(CURLOPT_POST, 1L);
    if (!status.ok()) return OnTransferError(std::move(status));
    status = handle_.SetOption(CURLOPT_POSTFIELDSIZE_LARGE,
                               static_cast<curl_off_t>(writev.size()));
    if (!status.ok()) return OnTransferError(std::move(status));
    status =
        handle_.SetOption(CURLOPT_READFUNCTION, &RestCurlRequestOnReadData);
    if (!status.ok()) return OnTransferError(std::move(status));
    status = handle_.SetOption(CURLOPT_READDATA, &writev);
    if (!status.ok()) return OnTransferError(std::move(status));
    SetHeader("Expect:");
    return MakeRequestImpl();
  }

  if (method == HttpMethod::kPut || method == HttpMethod::kPatch) {
    WriteVector writev{std::move(payload)};
    status =
        handle_.SetOption(CURLOPT_READFUNCTION, &RestCurlRequestOnReadData);
    if (!status.ok()) return OnTransferError(std::move(status));
    status = handle_.SetOption(CURLOPT_READDATA, &writev);
    if (!status.ok()) return OnTransferError(std::move(status));
    status = handle_.SetOption(CURLOPT_UPLOAD, 1L);
    if (!status.ok()) return OnTransferError(std::move(status));
    return MakeRequestImpl();
  }

  return Status{StatusCode::kInvalidArgument,
                absl::StrCat("Unknown method: ", method),
                {}};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

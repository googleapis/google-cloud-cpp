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

#include "google/cloud/storage/internal/curl_download_request.h"
#include "google/cloud/storage/internal/binary_data_as_debug_string.h"
#include "google/cloud/storage/internal/curl_wrappers.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/log.h"
#include <curl/multi.h>
#include <algorithm>
#include <cstring>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

extern "C" std::size_t CurlDownloadRequestWrite(char* ptr, size_t size,
                                                size_t nmemb, void* userdata) {
  auto* request = reinterpret_cast<CurlDownloadRequest*>(userdata);
  return request->WriteCallback(ptr, size, nmemb);
}

extern "C" std::size_t CurlDownloadRequestHeader(char* contents,
                                                 std::size_t size,
                                                 std::size_t nitems,
                                                 void* userdata) {
  auto* request = reinterpret_cast<CurlDownloadRequest*>(userdata);
  return request->HeaderCallback(contents, size, nitems);
}

// Note that TRACE-level messages are disabled by default, even in
// CMAKE_BUILD_TYPE=Debug builds. The level of detail created by the
// TRACE_STATE() macro is only needed by the library developers when
// troubleshooting this class.
#define TRACE_STATE()                                                       \
  GCP_LOG(TRACE) << __func__ << "(), buffer_size_=" << buffer_size_         \
                 << ", buffer_offset_=" << buffer_offset_                   \
                 << ", spill_.size()=" << spill_.size()                     \
                 << ", spill_offset_=" << spill_offset_                     \
                 << ", closing=" << closing_ << ", closed=" << curl_closed_ \
                 << ", paused=" << paused_ << ", in_multi=" << in_multi_

CurlDownloadRequest::CurlDownloadRequest()
    : headers_(nullptr, &curl_slist_free_all),
      download_stall_timeout_(0),
      multi_(nullptr, &curl_multi_cleanup),
      spill_(CURL_MAX_WRITE_SIZE) {}

template <typename Predicate>
Status CurlDownloadRequest::Wait(Predicate predicate) {
  int repeats = 0;
  // We can assert that the current thread is the leader, because the
  // predicate is satisfied, and the condition variable exited. Therefore,
  // this thread must run the I/O event loop.
  while (!predicate()) {
    handle_.FlushDebug(__func__);
    TRACE_STATE() << ", repeats=" << repeats;
    auto running_handles = PerformWork();
    if (!running_handles.ok()) {
      return std::move(running_handles).status();
    }
    // Only wait if there are CURL handles with pending work *and* the
    // predicate is not satisfied. Note that if the predicate is ill-defined
    // it might continue to be unsatisfied even though the handles have
    // completed their work.
    if (*running_handles == 0 || predicate()) {
      break;
    }
    auto status = WaitForHandles(repeats);
    if (!status.ok()) {
      return status;
    }
  }
  return Status();
}

StatusOr<HttpResponse> CurlDownloadRequest::Close() {
  TRACE_STATE();
  // Set the the closing_ flag to trigger a return 0 from the next read
  // callback, see the comments in the header file for more details.
  closing_ = true;

  (void)handle_.EasyPause(CURLPAUSE_RECV_CONT);
  paused_ = false;
  TRACE_STATE();

  // Block until that callback is made.
  auto status = Wait([this] { return curl_closed_; });
  if (!status.ok()) {
    TRACE_STATE() << ", status=" << status;
    return status;
  }
  TRACE_STATE();

  // Now remove the handle from the CURLM* interface and wait for the response.
  if (in_multi_) {
    auto error = curl_multi_remove_handle(multi_.get(), handle_.handle_.get());
    in_multi_ = false;
    status = AsStatus(error, __func__);
    if (!status.ok()) {
      TRACE_STATE() << ", status=" << status;
      return status;
    }
  }

  auto http_code = handle_.GetResponseCode();
  if (!http_code.ok()) {
    TRACE_STATE() << ", http_code.status=" << http_code.status();
    return http_code.status();
  }
  TRACE_STATE() << ", http_code.status=" << http_code.status()
                << ", http_code=" << *http_code;
  return HttpResponse{http_code.value(), std::string{},
                      std::move(received_headers_)};
}

StatusOr<ReadSourceResult> CurlDownloadRequest::Read(char* buf, std::size_t n) {
  buffer_ = buf;
  buffer_offset_ = 0;
  buffer_size_ = n;
  if (n == 0) {
    return Status(StatusCode::kInvalidArgument, "Empty buffer for Read()");
  }
  handle_.SetOption(CURLOPT_WRITEFUNCTION, &CurlDownloadRequestWrite);
  handle_.SetOption(CURLOPT_WRITEDATA, this);
  handle_.SetOption(CURLOPT_HEADERFUNCTION, &CurlDownloadRequestHeader);
  handle_.SetOption(CURLOPT_HEADERDATA, this);

  // Before calling `Wait()` copy any data from the spill buffer into the
  // application buffer. It is possible that `Wait()` will never call
  // `WriteCallback()`, for example, because the Read() or Peek() closed the
  // connection, but if there is any data left in the spill buffer we need
  // to return it.
  DrainSpillBuffer();

  handle_.FlushDebug(__func__);
  TRACE_STATE();

#if CURL_AT_LEAST_VERSION(7, 69, 0)
  if (!curl_closed_ && paused_) {
#else
  if (!curl_closed_) {
#endif  // libcurl >= 7.69.0
    auto status = handle_.EasyPause(CURLPAUSE_RECV_CONT);
    if (!status.ok()) {
      TRACE_STATE() << ", status=" << status;
      return status;
    }
    paused_ = false;
    TRACE_STATE();
  }

  auto status = Wait([this] {
    return curl_closed_ || paused_ || buffer_offset_ >= buffer_size_;
  });
  if (!status.ok()) {
    return status;
  }
  TRACE_STATE();
  auto bytes_read = buffer_offset_;
  buffer_ = nullptr;
  buffer_offset_ = 0;
  buffer_size_ = 0;
  if (curl_closed_) {
    // Retrieve the response code for a closed stream. Note the use of
    // `.value()`, this is equivalent to: assert(http_code.ok());
    // The only way the previous call can fail indicates a bug in our code (or
    // corrupted memory), the documentation for CURLINFO_RESPONSE_CODE:
    //   https://curl.haxx.se/libcurl/c/CURLINFO_RESPONSE_CODE.html
    // says:
    //   Returns CURLE_OK if the option is supported, and CURLE_UNKNOWN_OPTION
    //   if not.
    // if the option is not supported then we cannot use HTTP at all in libcurl
    // and the whole class would fail.
    HttpResponse response{handle_.GetResponseCode().value(), std::string{},
                          std::move(received_headers_)};
    TRACE_STATE() << ", code=" << response.status_code;
    status = google::cloud::storage::internal::AsStatus(response);
    if (!status.ok()) {
      TRACE_STATE() << ", status=" << response.status_code;
      return status;
    }
    return ReadSourceResult{bytes_read, std::move(response)};
  }
  TRACE_STATE() << ", code=100";
  return ReadSourceResult{
      bytes_read,
      HttpResponse{
          HttpStatusCode::kContinue, {}, std::move(received_headers_)}};
}

void CurlDownloadRequest::SetOptions() {
  // We get better performance using a slightly larger buffer (128KiB) than the
  // default buffer size set by libcurl (16KiB)
  auto constexpr kDefaultBufferSize = 128 * 1024L;

  handle_.SetOption(CURLOPT_URL, url_.c_str());
  handle_.SetOption(CURLOPT_HTTPHEADER, headers_.get());
  handle_.SetOption(CURLOPT_USERAGENT, user_agent_.c_str());
  handle_.SetOption(CURLOPT_NOSIGNAL, 1L);
  handle_.SetOption(CURLOPT_NOPROGRESS, 1L);
  handle_.SetOption(CURLOPT_BUFFERSIZE, kDefaultBufferSize);
  if (!payload_.empty()) {
    handle_.SetOption(CURLOPT_POSTFIELDSIZE, payload_.length());
    handle_.SetOption(CURLOPT_POSTFIELDS, payload_.c_str());
  }
  handle_.EnableLogging(logging_enabled_);
  handle_.SetSocketCallback(socket_options_);
  if (download_stall_timeout_.count() != 0) {
    // Timeout if the download receives less than 1 byte/second (i.e.
    // effectively no bytes) for `download_stall_timeout_` seconds.
    handle_.SetOption(CURLOPT_LOW_SPEED_LIMIT, 1L);
    handle_.SetOption(
        CURLOPT_LOW_SPEED_TIME,
        // NOLINTNEXTLINE(google-runtime-int) - libcurl *requires* `long`
        static_cast<long>(download_stall_timeout_.count()));
  }
  if (in_multi_) {
    return;
  }
  auto error = curl_multi_add_handle(multi_.get(), handle_.handle_.get());
  if (error != CURLM_OK) {
    // This indicates that we are using the API incorrectly, the application
    // can not recover from these problems, raising an exception is the
    // "Right Thing"[tm] here.
    google::cloud::internal::ThrowStatus(AsStatus(error, __func__));
  }
  in_multi_ = true;
}

void CurlDownloadRequest::DrainSpillBuffer() {
  std::size_t free = buffer_size_ - buffer_offset_;
  auto copy_count = (std::min)(free, spill_offset_);
  std::memcpy(buffer_ + buffer_offset_, spill_.data(), copy_count);
  buffer_offset_ += copy_count;
  std::memmove(spill_.data(), spill_.data() + copy_count,
               spill_.size() - copy_count);
  spill_offset_ -= copy_count;
}

std::size_t CurlDownloadRequest::WriteCallback(void* ptr, std::size_t size,
                                               std::size_t nmemb) {
  handle_.FlushDebug(__func__);
  TRACE_STATE() << ", n=" << size * nmemb;
  // This transfer is closing, just return zero, that will make libcurl finish
  // any pending work, and will return the handle_ pointer from
  // curl_multi_info_read() in PerformWork(). That is the point where
  // `curl_closed_` is set.
  if (closing_) {
    TRACE_STATE() << " closing";
    return 0;
  }
  if (buffer_offset_ >= buffer_size_) {
    TRACE_STATE() << " *** PAUSING HANDLE ***";
    paused_ = true;
    return CURL_READFUNC_PAUSE;
  }

  // Use the spill buffer first, if there is any...
  DrainSpillBuffer();
  std::size_t free = buffer_size_ - buffer_offset_;
  if (free == 0) {
    TRACE_STATE() << " *** PAUSING HANDLE ***";
    paused_ = true;
    return CURL_READFUNC_PAUSE;
  }
  TRACE_STATE() << ", n=" << size * nmemb << ", free=" << free;

  // Copy the full contents of `ptr` into the application buffer.
  if (size * nmemb < free) {
    std::memcpy(buffer_ + buffer_offset_, ptr, size * nmemb);
    buffer_offset_ += size * nmemb;
    TRACE_STATE() << ", n=" << size * nmemb;
    return size * nmemb;
  }
  // Copy as much as possible from `ptr` into the application buffer.
  std::memcpy(buffer_ + buffer_offset_, ptr, free);
  buffer_offset_ += free;
  spill_offset_ = size * nmemb - free;
  // The rest goes into the spill buffer.
  std::memcpy(spill_.data(), static_cast<char*>(ptr) + free, spill_offset_);
  TRACE_STATE() << ", n=" << size * nmemb << ", free=" << free;
  return size * nmemb;
}

std::size_t CurlDownloadRequest::HeaderCallback(char* contents,
                                                std::size_t size,
                                                std::size_t nitems) {
  return CurlAppendHeaderData(received_headers_, contents, size * nitems);
}

StatusOr<int> CurlDownloadRequest::PerformWork() {
  TRACE_STATE();
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
    TRACE_STATE() << ", status=" << status;
    return status;
  }
  if (running_handles == 0) {
    // The only way we get here is if the handle "completed", and therefore the
    // transfer either failed or was successful. Pull all the messages out of
    // the info queue until we get the message about our handle.
    int remaining;
    while (auto msg = curl_multi_info_read(multi_.get(), &remaining)) {
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
                    << ", running_handles=" << running_handles;
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
                    << ", multi_remove_status=" << multi_remove_status;

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
  TRACE_STATE() << ", running_handles=" << running_handles;
  return running_handles;
}

Status CurlDownloadRequest::WaitForHandles(int& repeats) {
  int const timeout_ms = 1;
  std::chrono::milliseconds const timeout(timeout_ms);
  int numfds = 0;
  CURLMcode result =
      curl_multi_wait(multi_.get(), nullptr, 0, timeout_ms, &numfds);
  TRACE_STATE() << ", numfds=" << numfds << ", result=" << result
                << ", repeats=" << repeats;
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

Status CurlDownloadRequest::AsStatus(CURLMcode result, char const* where) {
  if (result == CURLM_OK) {
    return Status();
  }
  std::ostringstream os;
  os << where << "(): unexpected error code in curl_multi_*, [" << result
     << "]=" << curl_multi_strerror(result);
  return Status(StatusCode::kUnknown, std::move(os).str());
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

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
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/log.h"
#include "google/cloud/storage/internal/binary_data_as_debug_string.h"
#include "google/cloud/storage/internal/curl_wrappers.h"
#include <curl/multi.h>
#include <algorithm>
#include <cstring>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

CurlDownloadRequest::CurlDownloadRequest(std::size_t initial_buffer_size)
    : headers_(nullptr, &curl_slist_free_all),
      multi_(nullptr, &curl_multi_cleanup),
      closing_(false),
      curl_closed_(false),
      initial_buffer_size_(initial_buffer_size) {
  buffer_.reserve(initial_buffer_size);
}

StatusOr<HttpResponse> CurlDownloadRequest::Close() {
  // Set the the closing_ flag to trigger a return 0 from the next read
  // callback, see the comments in the header file for more details.
  closing_ = true;
  // Block until that callback is made.
  auto status = Wait([this] { return curl_closed_; });
  if (not status.ok()) {
    return status;
  }

  // Now remove the handle from the CURLM* interface and wait for the response.
  auto error = curl_multi_remove_handle(multi_.get(), handle_.handle_.get());
  status = AsStatus(error, __func__);
  if (not status.ok()) {
    return status;
  }

  StatusOr<long> http_code = handle_.GetResponseCode();
  if (not http_code.ok()) {
    return http_code.status();
  }
  return HttpResponse{http_code.value(), std::string{},
                      std::move(received_headers_)};
}

StatusOr<HttpResponse> CurlDownloadRequest::GetMore(std::string& buffer) {
  handle_.FlushDebug(__func__);
  auto status = Wait([this] {
    return curl_closed_ or buffer_.size() >= initial_buffer_size_;
  });
  if (not status.ok()) {
    return status;
  }
  GCP_LOG(DEBUG) << __func__ << "(), curl.size=" << buffer_.size()
                 << ", closing=" << closing_ << ", closed=" << curl_closed_;
  if (curl_closed_) {
    // Remove the handle from the CURLM* interface and wait for the response.
    auto error = curl_multi_remove_handle(multi_.get(), handle_.handle_.get());
    status = AsStatus(error, __func__);
    if (not status.ok()) {
      return status;
    }

    buffer_.swap(buffer);
    buffer_.clear();
    StatusOr<long> http_code = handle_.GetResponseCode();
    if (not http_code.ok()) {
      return std::move(http_code).status();
    }
    GCP_LOG(DEBUG) << __func__ << "(), size=" << buffer.size()
                   << ", closing=" << closing_ << ", closed=" << curl_closed_
                   << ", code=" << *http_code;
    return HttpResponse{http_code.value(), std::string{},
                        std::move(received_headers_)};
  }
  buffer_.swap(buffer);
  buffer_.clear();
  buffer_.reserve(initial_buffer_size_);
  status = handle_.EasyPause(CURLPAUSE_RECV_CONT);
  if (not status.ok()) {
    return status;
  }
  GCP_LOG(DEBUG) << __func__ << "(), size=" << buffer.size()
                 << ", closing=" << closing_ << ", closed=" << curl_closed_
                 << ", code=100";
  return HttpResponse{100, {}, {}};
}

Status CurlDownloadRequest::SetOptions() {
  ResetOptions();
  auto error = curl_multi_add_handle(multi_.get(), handle_.handle_.get());
  return AsStatus(error, __func__);
}

void CurlDownloadRequest::ResetOptions() {
  if (curl_closed_) {
    // Once closed, we do not want to reset the request.
    return;
  }
  handle_.SetOption(CURLOPT_URL, url_.c_str());
  handle_.SetOption(CURLOPT_HTTPHEADER, headers_.get());
  handle_.SetOption(CURLOPT_USERAGENT, user_agent_.c_str());
  if (not payload_.empty()) {
    handle_.SetOption(CURLOPT_POSTFIELDSIZE, payload_.length());
    handle_.SetOption(CURLOPT_POSTFIELDS, payload_.c_str());
  }
  handle_.SetWriterCallback(
      [this](void* ptr, std::size_t size, std::size_t nmemb) {
        return this->WriteCallback(ptr, size, nmemb);
      });
  handle_.SetHeaderCallback([this](char* contents, std::size_t size,
                                   std::size_t nitems) {
    return CurlAppendHeaderData(
        received_headers_, static_cast<char const*>(contents), size * nitems);
  });
  handle_.EnableLogging(logging_enabled_);
}

std::size_t CurlDownloadRequest::WriteCallback(void* ptr, std::size_t size,
                                               std::size_t nmemb) {
  handle_.FlushDebug(__func__);
  GCP_LOG(DEBUG) << __func__ << "() size=" << size << ", nmemb=" << nmemb
                 << ", buffer.size=" << buffer_.size();
  // This transfer is closing, just return zero, that will make libcurl finish
  // any pending work, and will return the handle_ pointer from
  // curl_multi_info_read() in PerformWork(). That is the point where
  // `curl_closed_` is set.
  if (closing_) {
    return 0;
  }
  if (buffer_.size() >= initial_buffer_size_) {
    return CURL_READFUNC_PAUSE;
  }

  buffer_.append(static_cast<char const*>(ptr), size * nmemb);
  return size * nmemb;
}

StatusOr<int> CurlDownloadRequest::PerformWork() {
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
  if (not status.ok()) {
    return status;
  }
  if (running_handles == 0) {
    // The only way we get here is if the handle "completed", and therefore the
    // transfer either failed or was successful. Pull all the messages out of
    // the info queue until we get the message about our handle.
    int remaining;
    do {
      auto msg = curl_multi_info_read(multi_.get(), &remaining);
      if (msg->easy_handle != handle_.handle_.get()) {
        // Throw an exception if the handle is not the right one. This should
        // never happen, but better to give some meaningful error in this case.
        std::ostringstream os;
        os << __func__ << " unknown handle returned by curl_multi_info_read()"
           << ", msg.msg=[" << msg->msg << "]"
           << ", result=[" << msg->data.result
           << "]=" << curl_easy_strerror(msg->data.result);
        return Status(StatusCode::kUnknown, std::move(os).str());
      }
      GCP_LOG(DEBUG) << __func__ << "(): msg.msg=[" << msg->msg << "], "
                     << " result=[" << msg->data.result
                     << "]=" << curl_easy_strerror(msg->data.result);
      // The transfer is done, set the state flags appropriately.
      curl_closed_ = true;
    } while (remaining > 0);
  }
  return running_handles;
}

Status CurlDownloadRequest::WaitForHandles(int& repeats) {
  int const timeout_ms = 1;
  std::chrono::milliseconds const timeout(timeout_ms);
  int numfds = 0;
  CURLMcode result =
      curl_multi_wait(multi_.get(), nullptr, 0, timeout_ms, &numfds);
  GCP_LOG(DEBUG) << __func__ << "(): numfds=" << numfds << ", result=" << result
                 << ", repeats=" << repeats;
  Status status = AsStatus(result, __func__);
  if (not status.ok()) {
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

Status CurlDownloadRequest::AsStatus(CURLMcode result, char const *where) {
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

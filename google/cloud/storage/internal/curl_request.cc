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
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/log.h"
#include "google/cloud/storage/internal/curl_wrappers.h"
#include <cctype>
#include <iostream>
#include <sstream>

namespace {
std::string FormatData(char const* data, std::size_t size) {
  std::string result = "\n";
  std::size_t text_width = 24;
  std::string text_column(text_width, ' ');
  std::string hex_column(2 * text_width, ' ');

  auto flush = [&result, &text_column, &hex_column, text_width] {
    result += text_column;
    result += ' ';
    result += hex_column;
    result += '\n';
    text_column = std::string(text_width, ' ');
    hex_column = std::string(2 * text_width, ' ');
  };

  std::size_t count = 0;
  for (char const* c = data; c != data + size; ++c) {
    if (std::isprint(*c) != 0) {
      text_column[count] = *c;
    } else {
      text_column[count] = '.';
    }
    snprintf(&hex_column[2 * count], 3, "%02x", *c);
    ++count;
    if (count == text_width) {
      flush();
      count = 0;
    }
  }
  if (count != 0) {
    flush();
  }
  return result;
}

extern "C" int DebugCallback(CURL* /*handle*/, curl_infotype type, char* data,
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
      *debug_buffer += ">> curl(Recv Data): " + FormatData(data, size);
      break;
    case CURLINFO_DATA_OUT:
      *debug_buffer += ">> curl(Send Data): " + FormatData(data, size);
      break;
    case CURLINFO_SSL_DATA_IN:
    case CURLINFO_SSL_DATA_OUT:
      // Do not print SSL binary data because generally that is not useful.
    case CURLINFO_END:
      break;
  }
  return 0;
}
}  // namespace

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

CurlRequest::CurlRequest(std::string base_url)
    : url_(std::move(base_url)),
      query_parameter_separator_("?"),
      curl_(curl_easy_init()),
      headers_(nullptr) {
  if (curl_ == nullptr) {
    google::cloud::internal::RaiseRuntimeError("Cannot initialize CURL handle");
  }
}

CurlRequest::~CurlRequest() {
  curl_slist_free_all(headers_);
  curl_easy_cleanup(curl_);
  if (not debug_buffer_.empty()) {
    GCP_LOG(INFO) << "HTTP Trace\n" << debug_buffer_;
  }
}

void CurlRequest::AddHeader(std::string const& header) {
  headers_ = curl_slist_append(headers_, header.c_str());
}

void CurlRequest::AddQueryParameter(std::string const& key,
                                    std::string const& value) {
  std::string parameter = query_parameter_separator_;
  parameter += MakeEscapedString(key).get();
  parameter += "=";
  parameter += MakeEscapedString(value).get();
  query_parameter_separator_ = "&";
  url_.append(parameter);
}

void CurlRequest::PrepareRequest(std::string payload, bool enable_logging) {
  // Pre-compute and cache the user agent string:
  static std::string const user_agent = [] {
    std::string agent = "gcs-c++/";
    agent += storage::version_string();
    agent += ' ';
    agent += curl_version();
    return agent;
  }();

  curl_easy_setopt(curl_, CURLOPT_URL, url_.c_str());
  curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers_);
  curl_easy_setopt(curl_, CURLOPT_USERAGENT, user_agent.c_str());
  payload_ = std::move(payload);
  if (not payload_.empty()) {
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, payload_.length());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, payload_.c_str());
  }
  if (enable_logging) {
    curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1);
    curl_easy_setopt(curl_, CURLOPT_DEBUGDATA, &debug_buffer_);
    curl_easy_setopt(curl_, CURLOPT_DEBUGFUNCTION, DebugCallback);
  }
}

HttpResponse CurlRequest::MakeRequest() {
  response_payload_.Attach(curl_);
  response_headers_.Attach(curl_);

  auto error = curl_easy_perform(curl_);
  if (error != CURLE_OK) {
    auto buffer = response_payload_.contents();
    std::ostringstream os;
    os << "Error while performing curl request, " << curl_easy_strerror(error)
       << "[" << error << "]: " << buffer << std::endl;
    google::cloud::internal::RaiseRuntimeError(os.str());
  }

  long code;
  curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &code);
  return HttpResponse{code, response_payload_.contents(),
                      response_headers_.contents()};
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

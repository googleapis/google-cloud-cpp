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
#include "google/cloud/log.h"
#include "google/cloud/rest_options.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <vector>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;

TEST(SpillBuffer, InitialState) {
  SpillBuffer sb;
  EXPECT_EQ(sb.capacity(), CURL_MAX_WRITE_SIZE);
  EXPECT_EQ(sb.size(), 0);
}

TEST(SpillBuffer, Simple) {
  SpillBuffer sb;
  sb.CopyFrom(absl::MakeSpan("abc", 3));
  EXPECT_EQ(sb.size(), 3);
  std::vector<char> buf{'A', 'B', 'C', 'D'};
  EXPECT_EQ(sb.MoveTo(absl::MakeSpan(buf)), 3);
  EXPECT_EQ(sb.size(), 0);
  EXPECT_THAT(buf, ElementsAre('a', 'b', 'c', 'D'));
}

TEST(SpillBuffer, Wrap) {
  SpillBuffer sb;
  std::vector<char> buf(sb.capacity() - 1, 'X');

  sb.CopyFrom(absl::MakeSpan(buf));
  EXPECT_EQ(sb.size(), buf.size());
  EXPECT_EQ(sb.MoveTo(absl::MakeSpan(buf.data(), buf.size() - 1)),
            buf.size() - 1);
  EXPECT_EQ(sb.size(), 1);

  sb.CopyFrom(absl::MakeSpan("abc", 3));
  EXPECT_EQ(sb.size(), 4);
  EXPECT_EQ(sb.MoveTo(absl::MakeSpan(buf)), 4);
  EXPECT_EQ(sb.size(), 0);
  EXPECT_THAT((std::vector<char>{buf.data(), buf.data() + 4}),
              ElementsAre('X', 'a', 'b', 'c'));
}

class CurlImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    factory_ = GetDefaultCurlHandleFactory();
    handle_ = CurlHandle::MakeFromPool(*factory_);
  }

  CurlHandle handle_;
  std::shared_ptr<CurlHandleFactory> factory_;
};

TEST_F(CurlImplTest, SetUrlEndpointOnly) {
  auto impl = CurlImpl(std::move(handle_), factory_, {});
  impl.SetUrl("https://endpoint.googleapis.com", {}, {});
  EXPECT_THAT(impl.url(), Eq("https://endpoint.googleapis.com"));
}

TEST_F(CurlImplTest, SetUrlEndpointPath) {
  RestRequest request;
  request.SetPath("resource/verb");
  auto impl = CurlImpl(std::move(handle_), factory_, {});
  impl.SetUrl("https://endpoint.googleapis.com", request, {});
  EXPECT_THAT(impl.url(), Eq("https://endpoint.googleapis.com/resource/verb"));
}

TEST_F(CurlImplTest, SetUrlEndpointNoPathAdditionalParameters) {
  RestRequest::HttpParameters params;
  params.emplace_back("query", "foo");
  auto impl = CurlImpl(std::move(handle_), factory_, {});
  impl.SetUrl("https://endpoint.googleapis.com", {}, params);
  EXPECT_THAT(impl.url(), Eq("https://endpoint.googleapis.com/?query=foo"));
}

TEST_F(CurlImplTest, SetUrlEndpointPathAdditionalParameters) {
  RestRequest request;
  request.SetPath("resource/verb");
  RestRequest::HttpParameters params;
  params.emplace_back("query", "foo");
  auto impl = CurlImpl(std::move(handle_), factory_, {});
  impl.SetUrl("https://endpoint.googleapis.com", request, params);
  EXPECT_THAT(impl.url(),
              Eq("https://endpoint.googleapis.com/resource/verb?query=foo"));
}

TEST_F(CurlImplTest, SetUrlEndpointPathRequestParametersAdditionalParameters) {
  RestRequest request;
  request.SetPath("resource/verb");
  request.AddQueryParameter("sort", "desc");
  RestRequest::HttpParameters params;
  params.emplace_back("query", "foo");
  auto impl = CurlImpl(std::move(handle_), factory_, {});
  impl.SetUrl("https://endpoint.googleapis.com", request, params);
  EXPECT_THAT(
      impl.url(),
      Eq("https://endpoint.googleapis.com/resource/verb?sort=desc&query=foo"));
}

TEST_F(CurlImplTest,
       SetUrlEndpointPathParametersRequestParametersAdditionalParameters) {
  RestRequest request;
  request.SetPath("resource/verb?path=param");
  request.AddQueryParameter("sort", "desc");
  RestRequest::HttpParameters params;
  params.emplace_back("query", "foo");
  auto impl = CurlImpl(std::move(handle_), factory_, {});
  impl.SetUrl("https://endpoint.googleapis.com", request, params);
  EXPECT_THAT(impl.url(), Eq("https://endpoint.googleapis.com/resource/"
                             "verb?path=param&sort=desc&query=foo"));
}

TEST_F(CurlImplTest, SetUrlPathContainsHttps) {
  RestRequest request;
  request.SetPath("HTTPS://endpoint.googleapis.com/resource/verb");
  auto impl = CurlImpl(std::move(handle_), factory_, {});
  impl.SetUrl("https://endpoint.googleapis.com", request, {});
  EXPECT_THAT(impl.url(), Eq("HTTPS://endpoint.googleapis.com/resource/verb"));
}

TEST_F(CurlImplTest, SetUrlPathContainsHttp) {
  RestRequest request;
  request.SetPath("HTTP://endpoint.googleapis.com/resource/verb");
  auto impl = CurlImpl(std::move(handle_), factory_, {});
  impl.SetUrl("https://endpoint.googleapis.com", request, {});
  EXPECT_THAT(impl.url(), Eq("HTTP://endpoint.googleapis.com/resource/verb"));
}

TEST_F(CurlImplTest, CurlOptProxy) {
  EXPECT_EQ(CurlOptProxy(Options{}), absl::nullopt);
  EXPECT_EQ(CurlOptProxy(Options{}.set<ProxyOption>(
                ProxyConfig().set_hostname("hostname"))),
            absl::make_optional(std::string("https://hostname")));
  EXPECT_EQ(
      CurlOptProxy(Options{}.set<ProxyOption>(ProxyConfig()
                                                  .set_hostname("hostname")
                                                  .set_port("1080")
                                                  .set_scheme("http"))),
      absl::make_optional(std::string("htt" /*silence*/ "p://hostname:1080")));
}

TEST_F(CurlImplTest, CurlOptProxyUsername) {
  EXPECT_EQ(CurlOptProxyUsername(Options{}), absl::nullopt);
  EXPECT_EQ(CurlOptProxyUsername(Options{}.set<ProxyOption>(
                ProxyConfig().set_hostname("hostname"))),
            absl::nullopt);
  EXPECT_EQ(
      CurlOptProxyUsername(Options{}.set<ProxyOption>(
          ProxyConfig().set_hostname("hostname").set_username("username"))),
      absl::make_optional(std::string("username")));
}

TEST_F(CurlImplTest, CurlOptProxyPassword) {
  EXPECT_EQ(CurlOptProxyPassword(Options{}), absl::nullopt);
  EXPECT_EQ(CurlOptProxyPassword(Options{}.set<ProxyOption>(
                ProxyConfig().set_hostname("hostname"))),
            absl::nullopt);
  EXPECT_EQ(
      CurlOptProxyPassword(Options{}.set<ProxyOption>(
          ProxyConfig().set_hostname("hostname").set_password("password"))),
      absl::make_optional(std::string("password")));
}

TEST_F(CurlImplTest, CurlOptInterface) {
  EXPECT_EQ(CurlOptInterface(Options{}), absl::nullopt);
  EXPECT_EQ(CurlOptInterface(Options{}.set<Interface>("interface")),
            absl::make_optional(std::string("interface")));
}

TEST_F(CurlImplTest, MergeAndWriteHeadersEmpty) {
  std::vector<std::string> headers_written;
  auto write_fn = [&headers_written](HttpHeader const& header) {
    headers_written.push_back(std::string(header));
  };
  auto impl = CurlImpl(std::move(handle_), factory_, {});
  impl.MergeAndWriteHeaders(write_fn);
  EXPECT_THAT(headers_written, ElementsAre());
}

TEST_F(CurlImplTest, MergeAndWriteHeadersOneEntry) {
  std::vector<std::string> headers_written;
  auto write_fn = [&headers_written](HttpHeader const& header) {
    headers_written.push_back(std::string(header));
  };
  auto impl = CurlImpl(std::move(handle_), factory_, {});
  HttpHeader h("my-key", "my-value");
  impl.SetHeader(h);
  impl.MergeAndWriteHeaders(write_fn);
  EXPECT_THAT(headers_written, ElementsAre(std::string(h)));
}

TEST_F(CurlImplTest, MergeAndWriteHeadersTwoUniqueEntries) {
  std::vector<std::string> headers_written;
  auto write_fn = [&headers_written](HttpHeader const& header) {
    headers_written.push_back(std::string(header));
  };
  auto impl = CurlImpl(std::move(handle_), factory_, {});
  HttpHeader h("my-key", "my-value");
  impl.SetHeader(h);
  HttpHeader h2("my-key2", "my-value2");
  impl.SetHeader(h2);
  impl.MergeAndWriteHeaders(write_fn);
  EXPECT_THAT(headers_written, ElementsAre(std::string(h), std::string(h2)));
}

TEST_F(CurlImplTest, MergeAndWriteHeadersMoreThanTwoUniqueEntries) {
  std::vector<std::string> headers_written;
  auto write_fn = [&headers_written](HttpHeader const& header) {
    headers_written.push_back(std::string(header));
  };
  auto impl = CurlImpl(std::move(handle_), factory_, {});
  HttpHeader h("my-key", "my-value");
  impl.SetHeader(h);
  HttpHeader h2("my-key2", "my-value2");
  impl.SetHeader(h2);
  HttpHeader h3("my-key3", "my-value3");
  impl.SetHeader(h3);
  impl.MergeAndWriteHeaders(write_fn);
  EXPECT_THAT(headers_written,
              ElementsAre(std::string(h), std::string(h2), std::string(h3)));
}

TEST_F(CurlImplTest, MergeAndWriteHeadersTwoDuplicateEntries) {
  std::vector<std::string> headers_written;
  auto write_fn = [&headers_written](HttpHeader const& header) {
    headers_written.push_back(std::string(header));
  };

  auto impl = CurlImpl(std::move(handle_), factory_, {});
  HttpHeader h("my-key", "my-value");
  impl.SetHeader(h);
  HttpHeader h2("my-key", "my-value2");
  impl.SetHeader(h2);
  impl.MergeAndWriteHeaders(write_fn);
  HttpHeader expected("my-key", "my-value,my-value2");
  EXPECT_THAT(headers_written, ElementsAre(std::string(expected)));
}

TEST_F(CurlImplTest, MergeAndWriteHeadersMoreThanTwoDuplicateEntries) {
  std::vector<std::string> headers_written;
  auto write_fn = [&headers_written](HttpHeader const& header) {
    headers_written.push_back(std::string(header));
  };

  auto impl = CurlImpl(std::move(handle_), factory_, {});
  HttpHeader h("my-key", "my-value");
  impl.SetHeader(h);
  HttpHeader h2("my-key", "my-value2");
  impl.SetHeader(h2);
  HttpHeader h3("my-key", "my-value3");
  impl.SetHeader(h3);
  impl.MergeAndWriteHeaders(write_fn);
  HttpHeader expected("my-key", "my-value,my-value2,my-value3");
  EXPECT_THAT(headers_written, ElementsAre(std::string(expected)));
}

TEST_F(CurlImplTest, MergeAndWriteHeadersBeginningDuplicateEntries) {
  std::vector<std::string> headers_written;
  auto write_fn = [&headers_written](HttpHeader const& header) {
    headers_written.push_back(std::string(header));
  };

  auto impl = CurlImpl(std::move(handle_), factory_, {});
  HttpHeader i("i-key", "value");
  impl.SetHeader(i);
  HttpHeader h("h-key", "my-value");
  impl.SetHeader(h);
  HttpHeader k("k-key", "my-value");
  impl.SetHeader(k);
  HttpHeader h2("h-key", "my-value2");
  impl.SetHeader(h2);
  HttpHeader h3("h-key", "my-value3");
  impl.SetHeader(h3);

  impl.MergeAndWriteHeaders(write_fn);
  HttpHeader i_expected("i-key", "value");
  HttpHeader h_expected("h-key", "my-value,my-value2,my-value3");
  HttpHeader k_expected("k-key", "my-value");
  EXPECT_THAT(headers_written,
              ElementsAre(std::string(h_expected), std::string(i_expected),
                          std::string(k_expected)));
}

TEST_F(CurlImplTest, MergeAndWriteHeadersMiddleDuplicateEntries) {
  std::vector<std::string> headers_written;
  auto write_fn = [&headers_written](HttpHeader const& header) {
    headers_written.push_back(std::string(header));
  };

  auto impl = CurlImpl(std::move(handle_), factory_, {});
  HttpHeader i("i-key", "value");
  impl.SetHeader(i);
  HttpHeader h("h-key", "my-value");
  impl.SetHeader(h);
  HttpHeader k("k-key", "my-value");
  impl.SetHeader(k);
  HttpHeader i2("i-key", "my-value2");
  impl.SetHeader(i2);
  HttpHeader i3("i-key", "my-value3");
  impl.SetHeader(i3);

  impl.MergeAndWriteHeaders(write_fn);
  HttpHeader i_expected("i-key", "value,my-value2,my-value3");
  HttpHeader h_expected("h-key", "my-value");
  HttpHeader k_expected("k-key", "my-value");
  EXPECT_THAT(headers_written,
              ElementsAre(std::string(h_expected), std::string(i_expected),
                          std::string(k_expected)));
}

TEST_F(CurlImplTest, MergeAndWriteHeadersEndingDuplicateEntries) {
  std::vector<std::string> headers_written;
  auto write_fn = [&headers_written](HttpHeader const& header) {
    headers_written.push_back(std::string(header));
  };

  auto impl = CurlImpl(std::move(handle_), factory_, {});
  HttpHeader i("i-key", "value");
  impl.SetHeader(i);
  HttpHeader h("h-key", "my-value");
  impl.SetHeader(h);
  HttpHeader k("k-key", "my-value");
  impl.SetHeader(k);
  HttpHeader k2("k-key", "my-value2");
  impl.SetHeader(k2);
  HttpHeader k3("k-key", "my-value3");
  impl.SetHeader(k3);

  impl.MergeAndWriteHeaders(write_fn);
  HttpHeader i_expected("i-key", "value");
  HttpHeader h_expected("h-key", "my-value");
  HttpHeader k_expected("k-key", "my-value,my-value2,my-value3");
  EXPECT_THAT(headers_written,
              ElementsAre(std::string(h_expected), std::string(i_expected),
                          std::string(k_expected)));
}

TEST_F(CurlImplTest, MergeAndWriteHeadersDoNotMergeAuthorization) {
  std::vector<std::string> headers_written;
  auto write_fn = [&headers_written](HttpHeader const& header) {
    headers_written.push_back(std::string(header));
  };

  auto impl = CurlImpl(std::move(handle_), factory_, {});
  HttpHeader auth("Authorization", "Bearer my-token");
  impl.SetHeader(auth);
  HttpHeader auth2("Authorization", "Bearer my-token");
  impl.SetHeader(auth2);
  HttpHeader auth3("authorization", "Bearer my-token");
  impl.SetHeader(auth3);
  impl.MergeAndWriteHeaders(write_fn);
  HttpHeader expected("Authorization", "Bearer my-token");
  EXPECT_THAT(headers_written, ElementsAre(std::string(expected)));
}

TEST_F(CurlImplTest, MergeAndWriteHeadersDoNotMergeContentLength) {
  std::vector<std::string> headers_written;
  auto write_fn = [&headers_written](HttpHeader const& header) {
    headers_written.push_back(std::string(header));
  };

  auto impl = CurlImpl(std::move(handle_), factory_, {});
  HttpHeader auth("content-length", "42");
  impl.SetHeader(auth);
  HttpHeader auth2("Content-Length", "43");
  impl.SetHeader(auth2);
  HttpHeader auth3("content-length", "44");
  impl.SetHeader(auth3);
  impl.MergeAndWriteHeaders(write_fn);
  HttpHeader expected("content-length", "42");
  EXPECT_THAT(headers_written, ElementsAre(std::string(expected)));
}

TEST_F(CurlImplTest, MakeRequestSetsNoProxyAndLogsForMetadataUrl) {
  testing_util::ScopedLog log_capture;
  CurlImpl impl(std::move(handle_), factory_, Options{});
  RestRequest request;
  std::string metadata_url =
      "http://metadata.google.internal/computeMetadata/v1/token";
  impl.SetUrl(metadata_url, request, {});
  RestContext rest_context;
  Status request_status =
      impl.MakeRequest(CurlImpl::HttpMethod::kGet, rest_context, {});
  std::vector<std::string> log_lines = log_capture.ExtractLines();
  EXPECT_THAT(
      log_lines,
      Contains(HasSubstr(
          "Explicitly setting NOPROXY for 'metadata.google.internal'")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(metadata_url)));
}

TEST_F(CurlImplTest, MakeRequestDoesNotLogNoProxyForNonMetadataUrl) {
  testing_util::ScopedLog log_capture;
  CurlImpl impl(std::move(handle_), factory_, Options{});
  RestRequest request;
  std::string non_metadata_url = "https://example.com/api/v1/data";
  impl.SetUrl(non_metadata_url, request, {});

  RestContext rest_context;
  (void)impl.MakeRequest(CurlImpl::HttpMethod::kGet, rest_context, {});

  std::vector<std::string> log_lines = log_capture.ExtractLines();

  bool found_specific_noproxy_log = false;
  for (auto const& line : log_lines) {
    if (line.find("Explicitly setting NOPROXY") != std::string::npos &&
        line.find("metadata.google.internal") != std::string::npos) {
      found_specific_noproxy_log = true;
      break;
    }
  }
  EXPECT_FALSE(found_specific_noproxy_log);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

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

#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/oauth2/anonymous_credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::testing::Contains;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Pair;
using ::testing::StartsWith;

std::string HttpBinEndpoint() {
  return google::cloud::internal::GetEnv("HTTPBIN_ENDPOINT")
      .value_or("https://nghttp2.org/httpbin");
}

// The integration tests sometimes flake (e.g. DNS failures) if we do not have a
// retry loop.
StatusOr<HttpResponse> RetryMakeRequest(CurlRequest request,
                                        std::string const& payload = {}) {
  auto delay = std::chrono::seconds(1);
  StatusOr<HttpResponse> response;
  for (auto i = 0; i != 3; ++i) {
    response = request.MakeRequest(payload);
    if (response) return response;
    std::this_thread::sleep_for(delay);
    delay *= 2;
  }
  return response;
}

StatusOr<HttpResponse> RetryMakeUploadRequest(
    CurlRequest request, std::vector<absl::Span<char const>> const& payload) {
  auto delay = std::chrono::seconds(1);
  StatusOr<HttpResponse> response;
  for (auto i = 0; i != 3; ++i) {
    response = request.MakeUploadRequest(payload);
    if (response) return response;
    std::this_thread::sleep_for(delay);
    delay *= 2;
  }
  return response;
}

TEST(CurlRequestTest, SimpleGET) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/get",
                             storage::internal::GetDefaultCurlHandleFactory());
  builder.AddQueryParameter("foo", "foo1&&&foo2");
  builder.AddQueryParameter("bar", "bar1==bar2=");
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("charsets: utf-8");

  auto response = RetryMakeRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  auto parsed = nlohmann::json::parse(response->payload);
  auto args = parsed["args"];
  EXPECT_EQ("foo1&&&foo2", args["foo"].get<std::string>());
  EXPECT_EQ("bar1==bar2=", args["bar"].get<std::string>());
}

TEST(CurlRequestTest, FailedGET) {
  // This test fails if somebody manages to run a https server on port 0 (you
  // can't, but just documenting the assumptions in this test).
  CurlRequestBuilder builder("https://localhost:1/",
                             storage::internal::GetDefaultCurlHandleFactory());

  auto response = builder.BuildRequest().MakeRequest({});
  EXPECT_THAT(response, Not(IsOk()));
}

TEST(CurlRequestTest, RepeatedGET) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/get",
                             storage::internal::GetDefaultCurlHandleFactory());
  builder.AddQueryParameter("foo", "foo1&&&foo2");
  builder.AddQueryParameter("bar", "bar1==bar2=");
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("charsets: utf-8");

  // This retry overload is only used in this function.
  auto retry = [](CurlRequest& request) {
    auto delay = std::chrono::seconds(1);
    StatusOr<HttpResponse> response;
    for (auto i = 0; i != 3; ++i) {
      response = request.MakeRequest(std::string{});
      if (response) return response;
      std::this_thread::sleep_for(delay);
      delay *= 2;
    }
    return response;
  };

  auto request = builder.BuildRequest();
  auto response = retry(request);
  ASSERT_STATUS_OK(response);

  EXPECT_EQ(200, response->status_code);
  auto parsed = nlohmann::json::parse(response->payload);
  auto args = parsed["args"];
  EXPECT_EQ("foo1&&&foo2", args["foo"].get<std::string>());
  EXPECT_EQ("bar1==bar2=", args["bar"].get<std::string>());

  response = retry(request);
  EXPECT_EQ(200, response->status_code);
  parsed = nlohmann::json::parse(response->payload);
  args = parsed["args"];
  EXPECT_EQ("foo1&&&foo2", args["foo"].get<std::string>());
  EXPECT_EQ("bar1==bar2=", args["bar"].get<std::string>());
}

TEST(CurlRequestTest, SimplePOST) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/post",
                             storage::internal::GetDefaultCurlHandleFactory());
  std::vector<std::pair<std::string, std::string>> form_parameters = {
      {"foo", "foo1&foo2 foo3"},
      {"bar", "bar1-bar2"},
      {"baz", "baz=baz2"},
  };
  std::string data;
  char const* sep = "";
  for (auto const& p : form_parameters) {
    data += sep;
    data += builder.MakeEscapedString(p.first).get();
    data += '=';
    data += builder.MakeEscapedString(p.second).get();
    sep = "&";
  }
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("Content-Type: application/x-www-form-urlencoded");
  builder.AddHeader("charsets: utf-8");

  auto response = RetryMakeRequest(builder.BuildRequest(), data);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  auto parsed = nlohmann::json::parse(response->payload);
  auto form = parsed["form"];
  EXPECT_EQ("foo1&foo2 foo3", form["foo"].get<std::string>());
  EXPECT_EQ("bar1-bar2", form["bar"].get<std::string>());
  EXPECT_EQ("baz=baz2", form["baz"].get<std::string>());
}

TEST(CurlRequestTest, MultiBufferPUT) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/put",
                             storage::internal::GetDefaultCurlHandleFactory());
  builder.SetMethod("PUT");

  std::vector<std::string> lines = {
      {"line 1"},
      {"line 2"},
      {"line 3"},
  };
  ConstBufferSequence data;
  std::string nl = "\n";
  for (auto const& p : lines) {
    data.emplace_back(p.data(), p.size());
    data.emplace_back(nl.data(), nl.size());
  }
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("Content-Type: application/octet-stream");
  builder.AddHeader("charsets: utf-8");

  auto response = RetryMakeUploadRequest(builder.BuildRequest(), data);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  auto parsed = nlohmann::json::parse(response->payload);
  EXPECT_EQ("line 1\nline 2\nline 3\n", parsed["data"]);
}

TEST(CurlRequestTest, MultiBufferEmptyPUT) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/put",
                             storage::internal::GetDefaultCurlHandleFactory());
  builder.SetMethod("PUT");

  builder.AddHeader("Accept: application/json");
  builder.AddHeader("Content-Type: application/octet-stream");
  builder.AddHeader("charsets: utf-8");

  auto response = RetryMakeRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  auto parsed = nlohmann::json::parse(response->payload);
  EXPECT_TRUE(parsed["data"].get<std::string>().empty());
}

TEST(CurlRequestTest, MultiBufferLargePUT) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/put",
                             storage::internal::GetDefaultCurlHandleFactory());
  builder.SetMethod("PUT");

  std::vector<std::string> lines;
  auto constexpr kLineSize = 1024;
  // libcurl's CURLOPT_READFUNCTION provides at most CURL_MAX_READ_SIZE bytes,
  // use enough buffers to ensure we get more than one read callback.
#if CURL_AT_LEAST_VERSION(7, 53, 0)
  auto constexpr kLineCount = (2 * CURL_MAX_READ_SIZE) / kLineSize;
#else
  auto constexpr kLineCount = 512 * 1024 / kLineSize;
#endif  // CURL_AT_LEAST_VERSION >= 7.53.0
  for (int i = 0; i != kLineCount; ++i) {
    std::string line = std::to_string(i);
    line += ": ";
    line += std::string(kLineSize, '=');
    lines.push_back(std::move(line));
  }
  std::vector<absl::Span<char const>> data;
  std::string nl = "\n";
  for (auto const& p : lines) {
    data.emplace_back(p.data(), p.size());
    data.emplace_back(nl.data(), nl.size());
  }
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("Content-Type: application/octet-stream");
  builder.AddHeader("charsets: utf-8");

  auto response = RetryMakeUploadRequest(builder.BuildRequest(), data);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  auto parsed = nlohmann::json::parse(response->payload);
  std::vector<std::string> const actual = absl::StrSplit(
      parsed["data"].get<std::string>(), '\n', absl::SkipEmpty());
  EXPECT_THAT(actual, ElementsAreArray(lines));
}

TEST(CurlRequestTest, Handle404) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/status/404",
                             storage::internal::GetDefaultCurlHandleFactory());
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("charsets: utf-8");

  auto response = RetryMakeRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(404, response->status_code);
}

/// @test Verify the payload for error status is included in the return value.
TEST(CurlRequestTest, HandleTeapot) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/status/418",
                             storage::internal::GetDefaultCurlHandleFactory());
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("charsets: utf-8");

  auto response = RetryMakeRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(418, response->status_code);
  EXPECT_THAT(response->payload, HasSubstr("[ teapot ]"));
}

/// @test Verify the response includes the header values.
TEST(CurlRequestTest, CheckResponseHeaders) {
  // Test that headers are parsed correctly. We send capitalized headers
  // because some versions of httpbin capitalize and others do not, in real
  // code (as opposed to a test), we should search for headers in a
  // case-insensitive manner, but that is not the purpose of this test.
  CurlRequestBuilder builder(HttpBinEndpoint() +
                                 "/response-headers"
                                 "?X-Test-Foo=bar"
                                 "&X-Test-Empty",
                             storage::internal::GetDefaultCurlHandleFactory());
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("charsets: utf-8");

  auto response = RetryMakeRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  EXPECT_EQ(1, response->headers.count("x-test-empty"));
  EXPECT_EQ("", response->headers.find("x-test-empty")->second);
  EXPECT_LE(1U, response->headers.count("x-test-foo"));
  EXPECT_EQ("bar", response->headers.find("x-test-foo")->second);
}

/// @test Verify the user agent header.
TEST(CurlRequestTest, UserAgent) {
  // Test that headers are parsed correctly. We send capitalized headers
  // because some versions of httpbin capitalize and others do not, in real
  // code (as opposed to a test), we should search for headers in a
  // case-insensitive manner, but that is not the purpose of this test.
  // Also verifying the telemetry header is present.
  CurlRequestBuilder builder(HttpBinEndpoint() + "/headers",
                             storage::internal::GetDefaultCurlHandleFactory());
  auto options = google::cloud::Options{}.set<UserAgentProductsOption>(
      {"test-user-agent-prefix"});
  builder.ApplyClientOptions(options);
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("charsets: utf-8");

  auto response = RetryMakeRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  auto payload = nlohmann::json::parse(response->payload);
  ASSERT_EQ(1U, payload.count("headers"));
  auto headers = payload["headers"];
  EXPECT_THAT(headers.value("User-Agent", ""),
              HasSubstr("test-user-agent-prefix"));
  EXPECT_THAT(headers.value("User-Agent", ""), HasSubstr("gcloud-cpp/"));
}

/// @test Verify the HTTP Version option.
TEST(CurlRequestTest, HttpVersion) {
  struct Test {
    std::string version;
    std::string prefix;
  } cases[] = {
      // The HTTP version setting is a request, libcurl may choose a slightly
      // different version (e.g. 1.1 when 1.0 is requested).
      {"1.1", "http/1"},
      {"1.0", "http/1"},
      {"2", "http/"},  // HTTP/2 may not be compiled in
      {"", "http/"},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing with version=<" + test.version + ">");
    CurlRequestBuilder builder(
        HttpBinEndpoint() + "/get",
        storage::internal::GetDefaultCurlHandleFactory());
    auto options =
        google::cloud::Options{}.set<storage_experimental::HttpVersionOption>(
            test.version);
    builder.ApplyClientOptions(options);
    builder.AddHeader("Accept: application/json");
    builder.AddHeader("charsets: utf-8");

    auto response = RetryMakeRequest(builder.BuildRequest());
    ASSERT_STATUS_OK(response);
    EXPECT_EQ(200, response->status_code);
    EXPECT_THAT(response->headers, Contains(Pair(StartsWith(test.prefix), "")));
  }
}

/// @test Verify that the Projection parameter is included if set.
TEST(CurlRequestTest, WellKnownQueryParametersProjection) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/get",
                             storage::internal::GetDefaultCurlHandleFactory());
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("charsets: utf-8");
  builder.AddOption(storage::Projection("full"));

  auto response = RetryMakeRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  auto parsed = nlohmann::json::parse(response->payload);
  auto args = parsed["args"];
  EXPECT_EQ("full", args.value("projection", ""));
  // The other well known parameters are not set.
  EXPECT_EQ(0, args.count("userProject"));
  EXPECT_EQ(0, args.count("ifGenerationMatch"));
  EXPECT_EQ(0, args.count("ifGenerationNotMatch"));
  EXPECT_EQ(0, args.count("ifMetagenerationMatch"));
  EXPECT_EQ(0, args.count("ifMetagenerationNotMatch"));
}

/// @test Verify that the UserProject parameter is included if set.
TEST(CurlRequestTest, WellKnownQueryParametersUserProject) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/get",
                             storage::internal::GetDefaultCurlHandleFactory());
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("charsets: utf-8");
  builder.AddOption(storage::UserProject("a-project"));

  auto response = RetryMakeRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  auto parsed = nlohmann::json::parse(response->payload);
  auto args = parsed["args"];
  EXPECT_EQ("a-project", args.value("userProject", ""));
  // The other well known parameters are not set.
  EXPECT_EQ(0, args.count("projection"));
  EXPECT_EQ(0, args.count("ifGenerationMatch"));
  EXPECT_EQ(0, args.count("ifGenerationNotMatch"));
  EXPECT_EQ(0, args.count("ifMetagenerationMatch"));
  EXPECT_EQ(0, args.count("ifMetagenerationNotMatch"));
}

/// @test Verify that the IfGenerationMatch parameter is included if set.
TEST(CurlRequestTest, WellKnownQueryParametersIfGenerationMatch) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/get",
                             storage::internal::GetDefaultCurlHandleFactory());
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("charsets: utf-8");
  builder.AddOption(storage::IfGenerationMatch(42));

  auto response = RetryMakeRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  auto parsed = nlohmann::json::parse(response->payload);
  auto args = parsed["args"];
  EXPECT_EQ("42", args.value("ifGenerationMatch", ""));
  // The other well known parameters are not set.
  EXPECT_EQ(0, args.count("projection"));
  EXPECT_EQ(0, args.count("userProject"));
  EXPECT_EQ(0, args.count("ifGenerationNotMatch"));
  EXPECT_EQ(0, args.count("ifMetagenerationMatch"));
  EXPECT_EQ(0, args.count("ifMetagenerationNotMatch"));
}

/// @test Verify that the IfGenerationNotMatch parameter is included if set.
TEST(CurlRequestTest, WellKnownQueryParametersIfGenerationNotMatch) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/get",
                             storage::internal::GetDefaultCurlHandleFactory());
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("charsets: utf-8");
  builder.AddOption(storage::IfGenerationNotMatch(42));

  auto response = RetryMakeRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  auto parsed = nlohmann::json::parse(response->payload);
  auto args = parsed["args"];
  EXPECT_EQ("42", args.value("ifGenerationNotMatch", ""));
  // The other well known parameters are not set.
  EXPECT_EQ(0, args.count("projection"));
  EXPECT_EQ(0, args.count("userProject"));
  EXPECT_EQ(0, args.count("ifGenerationMatch"));
  EXPECT_EQ(0, args.count("ifMetagenerationMatch"));
  EXPECT_EQ(0, args.count("ifMetagenerationNotMatch"));
}

/// @test Verify that the IfMetagenerationMatch parameter is included if set.
TEST(CurlRequestTest, WellKnownQueryParametersIfMetagenerationMatch) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/get",
                             storage::internal::GetDefaultCurlHandleFactory());
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("charsets: utf-8");
  builder.AddOption(storage::IfMetagenerationMatch(42));

  auto response = RetryMakeRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  auto parsed = nlohmann::json::parse(response->payload);
  auto args = parsed["args"];
  EXPECT_EQ("42", args.value("ifMetagenerationMatch", ""));
  // The other well known parameters are not set.
  EXPECT_EQ(0, args.count("projection"));
  EXPECT_EQ(0, args.count("userProject"));
  EXPECT_EQ(0, args.count("ifGenerationMatch"));
  EXPECT_EQ(0, args.count("ifGenerationNotMatch"));
  EXPECT_EQ(0, args.count("ifMetagenerationNotMatch"));
}

/// @test Verify that the IfMetagenerationNotMatch parameter is included if set.
TEST(CurlRequestTest, WellKnownQueryParametersIfMetagenerationNotMatch) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/get",
                             storage::internal::GetDefaultCurlHandleFactory());
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("charsets: utf-8");
  builder.AddOption(storage::IfMetagenerationNotMatch(42));

  auto response = RetryMakeRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  auto parsed = nlohmann::json::parse(response->payload);
  auto args = parsed["args"];
  EXPECT_EQ("42", args.value("ifMetagenerationNotMatch", ""));
  // The other well known parameters are not set.
  EXPECT_EQ(0, args.count("projection"));
  EXPECT_EQ(0, args.count("userProject"));
  EXPECT_EQ(0, args.count("ifGenerationMatch"));
  EXPECT_EQ(0, args.count("ifGenerationNotMatch"));
  EXPECT_EQ(0, args.count("ifMetagenerationMatch"));
}

/// @test Verify that the well-known query parameters are included if set.
TEST(CurlRequestTest, WellKnownQueryParametersMultiple) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/get",
                             storage::internal::GetDefaultCurlHandleFactory());
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("charsets: utf-8");
  builder.AddOption(storage::UserProject("user-project-id"));
  builder.AddOption(storage::IfMetagenerationMatch(7));
  builder.AddOption(storage::IfGenerationNotMatch(42));

  auto response = RetryMakeRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  auto parsed = nlohmann::json::parse(response->payload);
  auto args = parsed["args"];
  EXPECT_EQ("user-project-id", args.value("userProject", ""));
  EXPECT_EQ("7", args.value("ifMetagenerationMatch", ""));
  EXPECT_EQ("42", args.value("ifGenerationNotMatch", ""));
  // The other well known parameters are not set.
  EXPECT_EQ(0, args.count("projection"));
  EXPECT_EQ(0, args.count("ifGenerationMatch"));
  EXPECT_EQ(0, args.count("ifMetagenerationNotMatch"));
}

class MockLogBackend : public google::cloud::LogBackend {
 public:
  void Process(google::cloud::LogRecord const& lr) override {
    ProcessWithOwnership(lr);
  }
  MOCK_METHOD(void, ProcessWithOwnership, (google::cloud::LogRecord),
              (override));
};

/// @test Verify that CurlRequest logs when requested.
TEST(CurlRequestTest, Logging) {
  // Prepare the Log subsystem to receive mock calls:
  auto mock_logger = std::make_shared<MockLogBackend>();
  auto backend_id = google::cloud::LogSink::Instance().AddBackend(mock_logger);

  std::string log_messages;
  EXPECT_CALL(*mock_logger, ProcessWithOwnership)
      .WillRepeatedly([&log_messages](google::cloud::LogRecord const& lr) {
        log_messages += lr.message;
        log_messages += "\n";
      });

  {
    CurlRequestBuilder builder(
        HttpBinEndpoint() + "/post?foo=bar",
        storage::internal::GetDefaultCurlHandleFactory());
    auto options =
        google::cloud::Options{}.set<TracingComponentsOption>({"http"});
    builder.ApplyClientOptions(options);
    builder.AddHeader("Accept: application/json");
    builder.AddHeader("charsets: utf-8");
    builder.AddHeader("x-test-header: foo");

    auto response =
        RetryMakeRequest(builder.BuildRequest(), "this is some text");
    ASSERT_STATUS_OK(response);
    EXPECT_EQ(200, response->status_code);
  }

  google::cloud::LogSink::Instance().RemoveBackend(backend_id);

  // Verify the URL, and headers, and payload are logged.
  EXPECT_THAT(log_messages, HasSubstr("/post?foo=bar"));
  EXPECT_THAT(log_messages, HasSubstr("x-test-header: foo"));
  EXPECT_THAT(log_messages, HasSubstr("this is some text"));
  EXPECT_THAT(log_messages, HasSubstr("curl(Info)"));
  EXPECT_THAT(log_messages, HasSubstr("curl(Send Header)"));
  EXPECT_THAT(log_messages, HasSubstr("curl(Send Data)"));
  EXPECT_THAT(log_messages, HasSubstr("curl(Recv Header)"));
  EXPECT_THAT(log_messages, HasSubstr("curl(Recv Data)"));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

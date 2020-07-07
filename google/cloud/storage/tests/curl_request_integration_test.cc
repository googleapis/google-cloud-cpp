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
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/oauth2/anonymous_credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <cstdlib>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::ElementsAreArray;
using ::testing::HasSubstr;

std::string HttpBinEndpoint() {
  return google::cloud::internal::GetEnv("HTTPBIN_ENDPOINT")
      .value_or("https://nghttp2.org/httpbin");
}

TEST(CurlRequestTest, SimpleGET) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/get",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddQueryParameter("foo", "foo1&&&foo2");
  request.AddQueryParameter("bar", "bar1==bar2=");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  auto response = request.BuildRequest().MakeRequest(std::string{});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  nl::json parsed = nl::json::parse(response->payload);
  nl::json args = parsed["args"];
  EXPECT_EQ("foo1&&&foo2", args["foo"].get<std::string>());
  EXPECT_EQ("bar1==bar2=", args["bar"].get<std::string>());
}

TEST(CurlRequestTest, FailedGET) {
  // This test fails if somebody manages to run a https server on port 0 (you
  // can't, but just documenting the assumptions in this test).
  storage::internal::CurlRequestBuilder request(
      "https://localhost:1/", storage::internal::GetDefaultCurlHandleFactory());

  auto response = request.BuildRequest().MakeRequest(std::string{});
  EXPECT_FALSE(response.ok());
}

TEST(CurlRequestTest, RepeatedGET) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/get",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddQueryParameter("foo", "foo1&&&foo2");
  request.AddQueryParameter("bar", "bar1==bar2=");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  auto req = request.BuildRequest();
  auto response = req.MakeRequest(std::string{});
  ASSERT_STATUS_OK(response);

  EXPECT_EQ(200, response->status_code);
  nl::json parsed = nl::json::parse(response->payload);
  nl::json args = parsed["args"];
  EXPECT_EQ("foo1&&&foo2", args["foo"].get<std::string>());
  EXPECT_EQ("bar1==bar2=", args["bar"].get<std::string>());

  response = req.MakeRequest(std::string{});
  EXPECT_EQ(200, response->status_code);
  parsed = nl::json::parse(response->payload);
  args = parsed["args"];
  EXPECT_EQ("foo1&&&foo2", args["foo"].get<std::string>());
  EXPECT_EQ("bar1==bar2=", args["bar"].get<std::string>());
}

TEST(CurlRequestTest, SimplePOST) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/post",
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
    data += request.MakeEscapedString(p.first).get();
    data += '=';
    data += request.MakeEscapedString(p.second).get();
    sep = "&";
  }
  request.AddHeader("Accept: application/json");
  request.AddHeader("Content-Type: application/x-www-form-urlencoded");
  request.AddHeader("charsets: utf-8");

  auto response = request.BuildRequest().MakeRequest(data);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  nl::json parsed = nl::json::parse(response->payload);
  nl::json form = parsed["form"];
  EXPECT_EQ("foo1&foo2 foo3", form["foo"].get<std::string>());
  EXPECT_EQ("bar1-bar2", form["bar"].get<std::string>());
  EXPECT_EQ("baz=baz2", form["baz"].get<std::string>());
}

TEST(CurlRequestTest, MultiBufferPUT) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/put",
      storage::internal::GetDefaultCurlHandleFactory());
  request.SetMethod("PUT");

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
  request.AddHeader("Accept: application/json");
  request.AddHeader("Content-Type: application/octet-stream");
  request.AddHeader("charsets: utf-8");

  auto response = request.BuildRequest().MakeUploadRequest(data);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  nl::json parsed = nl::json::parse(response->payload);
  EXPECT_EQ("line 1\nline 2\nline 3\n", parsed["data"]);
}

TEST(CurlRequestTest, MultiBufferEmptyPUT) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/put",
      storage::internal::GetDefaultCurlHandleFactory());
  request.SetMethod("PUT");

  request.AddHeader("Accept: application/json");
  request.AddHeader("Content-Type: application/octet-stream");
  request.AddHeader("charsets: utf-8");

  auto response = request.BuildRequest().MakeUploadRequest({});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  nl::json parsed = nl::json::parse(response->payload);
  EXPECT_TRUE(parsed["data"].get<std::string>().empty());
}

TEST(CurlRequestTest, MultiBufferLargePUT) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/put",
      storage::internal::GetDefaultCurlHandleFactory());
  request.SetMethod("PUT");

  std::vector<std::string> lines;
  auto constexpr kLineSize = 1024;
  // libcurl's CURLOPT_READFUNCTION provides at most CURL_MAX_READ_SIZE bytes,
  // use enough buffers to ensure we get more than one read callback.
  auto constexpr kLineCount = (2 * CURL_MAX_READ_SIZE) / kLineSize;
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
  request.AddHeader("Accept: application/json");
  request.AddHeader("Content-Type: application/octet-stream");
  request.AddHeader("charsets: utf-8");

  auto response = request.BuildRequest().MakeUploadRequest(data);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  nl::json parsed = nl::json::parse(response->payload);
  std::vector<std::string> const actual = absl::StrSplit(
      parsed["data"].get<std::string>(), '\n', absl::SkipEmpty());
  EXPECT_THAT(actual, ElementsAreArray(lines));
}

TEST(CurlRequestTest, Handle404) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/status/404",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  auto response = request.BuildRequest().MakeRequest(std::string{});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(404, response->status_code);
}

/// @test Verify the payload for error status is included in the return value.
TEST(CurlRequestTest, HandleTeapot) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/status/418",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  auto response = request.BuildRequest().MakeRequest(std::string{});
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
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() +
          "/response-headers"
          "?X-Test-Foo=bar"
          "&X-Test-Empty",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  auto response = request.BuildRequest().MakeRequest(std::string{});
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
  storage::internal::CurlRequestBuilder builder(
      HttpBinEndpoint() + "/headers",
      storage::internal::GetDefaultCurlHandleFactory());
  auto options =
      ClientOptions(std::make_shared<storage::oauth2::AnonymousCredentials>())
          .add_user_agent_prefix("test-user-agent-prefix");
  builder.ApplyClientOptions(options);
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("charsets: utf-8");

  auto response = builder.BuildRequest().MakeRequest(std::string{});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  auto payload = nl::json::parse(response->payload);
  ASSERT_EQ(1U, payload.count("headers"));
  auto headers = payload["headers"];
  EXPECT_THAT(headers.value("User-Agent", ""),
              HasSubstr("test-user-agent-prefix"));
  EXPECT_THAT(headers.value("User-Agent", ""), HasSubstr("gcloud-cpp/"));
}

/// @test Verify that the Projection parameter is included if set.
TEST(CurlRequestTest, WellKnownQueryParametersProjection) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/get",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");
  request.AddOption(storage::Projection("full"));

  auto response = request.BuildRequest().MakeRequest(std::string{});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  nl::json parsed = nl::json::parse(response->payload);
  nl::json args = parsed["args"];
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
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/get",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");
  request.AddOption(storage::UserProject("a-project"));

  auto response = request.BuildRequest().MakeRequest(std::string{});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  nl::json parsed = nl::json::parse(response->payload);
  nl::json args = parsed["args"];
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
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/get",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");
  request.AddOption(storage::IfGenerationMatch(42));

  auto response = request.BuildRequest().MakeRequest(std::string{});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  nl::json parsed = nl::json::parse(response->payload);
  nl::json args = parsed["args"];
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
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/get",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");
  request.AddOption(storage::IfGenerationNotMatch(42));

  auto response = request.BuildRequest().MakeRequest(std::string{});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  nl::json parsed = nl::json::parse(response->payload);
  nl::json args = parsed["args"];
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
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/get",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");
  request.AddOption(storage::IfMetagenerationMatch(42));

  auto response = request.BuildRequest().MakeRequest(std::string{});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  nl::json parsed = nl::json::parse(response->payload);
  nl::json args = parsed["args"];
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
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/get",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");
  request.AddOption(storage::IfMetagenerationNotMatch(42));

  auto response = request.BuildRequest().MakeRequest(std::string{});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  nl::json parsed = nl::json::parse(response->payload);
  nl::json args = parsed["args"];
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
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/get",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");
  request.AddOption(storage::UserProject("user-project-id"));
  request.AddOption(storage::IfMetagenerationMatch(7));
  request.AddOption(storage::IfGenerationNotMatch(42));

  auto response = request.BuildRequest().MakeRequest(std::string{});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);
  nl::json parsed = nl::json::parse(response->payload);
  nl::json args = parsed["args"];
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
  google::cloud::LogSink::Instance().AddBackend(mock_logger);

  using ::testing::_;

  std::string log_messages;
  EXPECT_CALL(*mock_logger, ProcessWithOwnership(_))
      .WillRepeatedly([&log_messages](google::cloud::LogRecord const& lr) {
        log_messages += lr.message;
        log_messages += "\n";
      });

  {
    storage::internal::CurlRequestBuilder request(
        HttpBinEndpoint() + "/post?foo=bar",
        storage::internal::GetDefaultCurlHandleFactory());
    auto options =
        ClientOptions(std::make_shared<storage::oauth2::AnonymousCredentials>())
            .set_enable_http_tracing(true);
    request.ApplyClientOptions(options);
    request.AddHeader("Accept: application/json");
    request.AddHeader("charsets: utf-8");
    request.AddHeader("x-test-header: foo");

    auto response = request.BuildRequest().MakeRequest("this is some text");
    ASSERT_STATUS_OK(response);
    EXPECT_EQ(200, response->status_code);
  }

  google::cloud::LogSink::Instance().ClearBackends();

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

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

#include "google/cloud/log.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/nljson.h"
#include <gmock/gmock.h>
#include <cstdlib>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using testing::HasSubstr;

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
  EXPECT_EQ(200, response.status_code);
  nl::json parsed = nl::json::parse(response.payload);
  nl::json args = parsed["args"];
  EXPECT_EQ("foo1&&&foo2", args["foo"].get<std::string>());
  EXPECT_EQ("bar1==bar2=", args["bar"].get<std::string>());
}

TEST(CurlRequestTest, FailedGET) {
  // This test fails if somebody manages to run a https server on port 0 (you
  // can't, but just documenting the assumptions in this test).
  storage::internal::CurlRequestBuilder request(
      "https://localhost:0/", storage::internal::GetDefaultCurlHandleFactory());

  auto req = request.BuildRequest();
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(req.MakeRequest(std::string{}), std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(req.MakeRequest(std::string{}),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
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

  EXPECT_EQ(200, response.status_code);
  nl::json parsed = nl::json::parse(response.payload);
  nl::json args = parsed["args"];
  EXPECT_EQ("foo1&&&foo2", args["foo"].get<std::string>());
  EXPECT_EQ("bar1==bar2=", args["bar"].get<std::string>());

  response = req.MakeRequest(std::string{});
  EXPECT_EQ(200, response.status_code);
  parsed = nl::json::parse(response.payload);
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
  EXPECT_EQ(200, response.status_code);
  nl::json parsed = nl::json::parse(response.payload);
  nl::json form = parsed["form"];
  EXPECT_EQ("foo1&foo2 foo3", form["foo"].get<std::string>());
  EXPECT_EQ("bar1-bar2", form["bar"].get<std::string>());
  EXPECT_EQ("baz=baz2", form["baz"].get<std::string>());
}

TEST(CurlRequestTest, Handle404) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/status/404",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  auto response = request.BuildRequest().MakeRequest(std::string{});
  EXPECT_EQ(404, response.status_code);
}

/// @test Verify the payload for error status is included in the return value.
TEST(CurlRequestTest, HandleTeapot) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/status/418",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  auto response = request.BuildRequest().MakeRequest(std::string{});
  EXPECT_EQ(418, response.status_code);
  EXPECT_THAT(response.payload, HasSubstr("[ teapot ]"));
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
  EXPECT_EQ(200, response.status_code);
  EXPECT_EQ(1U, response.headers.count("x-test-empty"));
  EXPECT_EQ("", response.headers.find("x-test-empty")->second);
  EXPECT_LE(1U, response.headers.count("x-test-foo"));
  EXPECT_EQ("bar", response.headers.find("x-test-foo")->second);
}

/// @test Verify the user agent prefix affects the request.
TEST(CurlRequestTest, UserAgentPrefix) {
  // Test that headers are parsed correctly. We send capitalized headers
  // because some versions of httpbin capitalize and others do not, in real
  // code (as opposed to a test), we should search for headers in a
  // case-insensitive manner, but that is not the purpose of this test.
  storage::internal::CurlRequestBuilder builder(
      HttpBinEndpoint() + "/headers",
      storage::internal::GetDefaultCurlHandleFactory());
  builder.AddUserAgentPrefix("test-program");
  builder.AddHeader("Accept: application/json");
  builder.AddHeader("charsets: utf-8");

  auto response = builder.BuildRequest().MakeRequest(std::string{});
  EXPECT_EQ(200, response.status_code);
  auto payload = nl::json::parse(response.payload);
  ASSERT_EQ(1U, payload.count("headers"));
  auto headers = payload["headers"];
  EXPECT_THAT(headers.value("User-Agent", ""), HasSubstr("test-program"));
}

/// @test Verify that the Projection parameter is included if set.
TEST(CurlRequestTest, WellKnownQueryParameters_Projection) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/get",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");
  request.AddOption(storage::Projection("full"));

  auto response = request.BuildRequest().MakeRequest(std::string{});
  EXPECT_EQ(200, response.status_code);
  nl::json parsed = nl::json::parse(response.payload);
  nl::json args = parsed["args"];
  EXPECT_EQ("full", args.value("projection", ""));
  // The other well known parameters are not set.
  EXPECT_EQ(0U, args.count("userProject"));
  EXPECT_EQ(0U, args.count("ifGenerationMatch"));
  EXPECT_EQ(0U, args.count("ifGenerationNotMatch"));
  EXPECT_EQ(0U, args.count("ifMetagenerationMatch"));
  EXPECT_EQ(0U, args.count("ifMetagenerationNotMatch"));
}

/// @test Verify that the UserProject parameter is included if set.
TEST(CurlRequestTest, WellKnownQueryParameters_UserProject) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/get",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");
  request.AddOption(storage::UserProject("a-project"));

  auto response = request.BuildRequest().MakeRequest(std::string{});
  EXPECT_EQ(200, response.status_code);
  nl::json parsed = nl::json::parse(response.payload);
  nl::json args = parsed["args"];
  EXPECT_EQ("a-project", args.value("userProject", ""));
  // The other well known parameters are not set.
  EXPECT_EQ(0U, args.count("projection"));
  EXPECT_EQ(0U, args.count("ifGenerationMatch"));
  EXPECT_EQ(0U, args.count("ifGenerationNotMatch"));
  EXPECT_EQ(0U, args.count("ifMetagenerationMatch"));
  EXPECT_EQ(0U, args.count("ifMetagenerationNotMatch"));
}

/// @test Verify that the IfGenerationMatch parameter is included if set.
TEST(CurlRequestTest, WellKnownQueryParameters_IfGenerationMatch) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/get",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");
  request.AddOption(storage::IfGenerationMatch(42));

  auto response = request.BuildRequest().MakeRequest(std::string{});
  EXPECT_EQ(200, response.status_code);
  nl::json parsed = nl::json::parse(response.payload);
  nl::json args = parsed["args"];
  EXPECT_EQ("42", args.value("ifGenerationMatch", ""));
  // The other well known parameters are not set.
  EXPECT_EQ(0U, args.count("projection"));
  EXPECT_EQ(0U, args.count("userProject"));
  EXPECT_EQ(0U, args.count("ifGenerationNotMatch"));
  EXPECT_EQ(0U, args.count("ifMetagenerationMatch"));
  EXPECT_EQ(0U, args.count("ifMetagenerationNotMatch"));
}

/// @test Verify that the IfGenerationNotMatch parameter is included if set.
TEST(CurlRequestTest, WellKnownQueryParameters_IfGenerationNotMatch) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/get",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");
  request.AddOption(storage::IfGenerationNotMatch(42));

  auto response = request.BuildRequest().MakeRequest(std::string{});
  EXPECT_EQ(200, response.status_code);
  nl::json parsed = nl::json::parse(response.payload);
  nl::json args = parsed["args"];
  EXPECT_EQ("42", args.value("ifGenerationNotMatch", ""));
  // The other well known parameters are not set.
  EXPECT_EQ(0U, args.count("projection"));
  EXPECT_EQ(0U, args.count("userProject"));
  EXPECT_EQ(0U, args.count("ifGenerationMatch"));
  EXPECT_EQ(0U, args.count("ifMetagenerationMatch"));
  EXPECT_EQ(0U, args.count("ifMetagenerationNotMatch"));
}

/// @test Verify that the IfMetagenerationMatch parameter is included if set.
TEST(CurlRequestTest, WellKnownQueryParameters_IfMetagenerationMatch) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/get",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");
  request.AddOption(storage::IfMetagenerationMatch(42));

  auto response = request.BuildRequest().MakeRequest(std::string{});
  EXPECT_EQ(200, response.status_code);
  nl::json parsed = nl::json::parse(response.payload);
  nl::json args = parsed["args"];
  EXPECT_EQ("42", args.value("ifMetagenerationMatch", ""));
  // The other well known parameters are not set.
  EXPECT_EQ(0U, args.count("projection"));
  EXPECT_EQ(0U, args.count("userProject"));
  EXPECT_EQ(0U, args.count("ifGenerationMatch"));
  EXPECT_EQ(0U, args.count("ifGenerationNotMatch"));
  EXPECT_EQ(0U, args.count("ifMetagenerationNotMatch"));
}

/// @test Verify that the IfMetagenerationNotMatch parameter is included if set.
TEST(CurlRequestTest, WellKnownQueryParameters_IfMetagenerationNotMatch) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/get",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");
  request.AddOption(storage::IfMetagenerationNotMatch(42));

  auto response = request.BuildRequest().MakeRequest(std::string{});
  EXPECT_EQ(200, response.status_code);
  nl::json parsed = nl::json::parse(response.payload);
  nl::json args = parsed["args"];
  EXPECT_EQ("42", args.value("ifMetagenerationNotMatch", ""));
  // The other well known parameters are not set.
  EXPECT_EQ(0U, args.count("projection"));
  EXPECT_EQ(0U, args.count("userProject"));
  EXPECT_EQ(0U, args.count("ifGenerationMatch"));
  EXPECT_EQ(0U, args.count("ifGenerationNotMatch"));
  EXPECT_EQ(0U, args.count("ifMetagenerationMatch"));
}

/// @test Verify that the well-known query parameters are included if set.
TEST(CurlRequestTest, WellKnownQueryParameters_Multiple) {
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/get",
      storage::internal::GetDefaultCurlHandleFactory());
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");
  request.AddOption(storage::UserProject("user-project-id"));
  request.AddOption(storage::IfMetagenerationMatch(7));
  request.AddOption(storage::IfGenerationNotMatch(42));

  auto response = request.BuildRequest().MakeRequest(std::string{});
  EXPECT_EQ(200, response.status_code);
  nl::json parsed = nl::json::parse(response.payload);
  nl::json args = parsed["args"];
  EXPECT_EQ("user-project-id", args.value("userProject", ""));
  EXPECT_EQ("7", args.value("ifMetagenerationMatch", ""));
  EXPECT_EQ("42", args.value("ifGenerationNotMatch", ""));
  // The other well known parameters are not set.
  EXPECT_EQ(0U, args.count("projection"));
  EXPECT_EQ(0U, args.count("ifGenerationMatch"));
  EXPECT_EQ(0U, args.count("ifMetagenerationNotMatch"));
}

class MockLogBackend : public google::cloud::LogBackend {
 public:
  void Process(google::cloud::LogRecord const& lr) { ProcessWithOwnership(lr); }
  MOCK_METHOD1(ProcessWithOwnership, void(google::cloud::LogRecord));
};

/// @test Verify that CurlRequest logs when requested.
TEST(CurlRequestTest, Logging) {
  // Prepare the Log subsystem to receive mock calls:
  auto mock_logger = std::make_shared<MockLogBackend>();
  google::cloud::LogSink::Instance().AddBackend(mock_logger);

  using testing::_;
  using testing::Invoke;

  std::string log_messages;
  EXPECT_CALL(*mock_logger, ProcessWithOwnership(_))
      .WillRepeatedly(Invoke([&log_messages](google::cloud::LogRecord lr) {
        log_messages += lr.message;
        log_messages += "\n";
      }));

  {
    storage::internal::CurlRequestBuilder request(
        HttpBinEndpoint() + "/post?foo=bar",
        storage::internal::GetDefaultCurlHandleFactory());
    request.SetDebugLogging(true);
    request.AddHeader("Accept: application/json");
    request.AddHeader("charsets: utf-8");
    request.AddHeader("x-test-header: foo");

    auto response = request.BuildRequest().MakeRequest("this is some text");
    EXPECT_EQ(200, response.status_code);
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

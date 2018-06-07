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
#include <gmock/gmock.h>
#include <cstdlib>
#include <vector>

namespace nl = storage::internal::nl;
using testing::HasSubstr;

namespace {
std::string HttpBinEndpoint() {
  auto env = std::getenv("HTTPBIN_ENDPOINT");
  if (env != nullptr) {
    return env;
  }
  return "https://nghttp2.org/httpbin";
}
}  // namespace

TEST(CurlRequestTest, SimpleGET) {
  storage::internal::CurlRequest request(HttpBinEndpoint() + "/get");
  request.AddQueryParameter("foo", "foo1&&&foo2");
  request.AddQueryParameter("bar", "bar1==bar2=");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  request.PrepareRequest(std::string{});
  auto response = request.MakeRequest();
  EXPECT_EQ(200, response.status_code);
  nl::json parsed = nl::json::parse(response.payload);
  nl::json args = parsed["args"];
  EXPECT_EQ("foo1&&&foo2", args["foo"].get<std::string>());
  EXPECT_EQ("bar1==bar2=", args["bar"].get<std::string>());
}

TEST(CurlRequestTest, FailedGET) {
  // This test fails if somebody manages to run a https server on port 0 (you
  // can't, but just documenting the assumptions in this test).
  storage::internal::CurlRequest request("https://localhost:0/");

  request.PrepareRequest(std::string{});
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(request.MakeRequest(), std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(request.MakeRequest(), "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(CurlRequestTest, RepeatedGET) {
  storage::internal::CurlRequest request(HttpBinEndpoint() + "/get");
  request.AddQueryParameter("foo", "foo1&&&foo2");
  request.AddQueryParameter("bar", "bar1==bar2=");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  request.PrepareRequest(std::string{});

  auto response = request.MakeRequest();
  EXPECT_EQ(200, response.status_code);
  nl::json parsed = nl::json::parse(response.payload);
  nl::json args = parsed["args"];
  EXPECT_EQ("foo1&&&foo2", args["foo"].get<std::string>());
  EXPECT_EQ("bar1==bar2=", args["bar"].get<std::string>());

  response = request.MakeRequest();
  EXPECT_EQ(200, response.status_code);
  parsed = nl::json::parse(response.payload);
  args = parsed["args"];
  EXPECT_EQ("foo1&&&foo2", args["foo"].get<std::string>());
  EXPECT_EQ("bar1==bar2=", args["bar"].get<std::string>());
}

TEST(CurlRequestTest, SimplePOST) {
  storage::internal::CurlRequest request(HttpBinEndpoint() + "/post");
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

  request.PrepareRequest(data);
  auto response = request.MakeRequest();
  EXPECT_EQ(200, response.status_code);
  nl::json parsed = nl::json::parse(response.payload);
  nl::json form = parsed["form"];
  EXPECT_EQ("foo1&foo2 foo3", form["foo"].get<std::string>());
  EXPECT_EQ("bar1-bar2", form["bar"].get<std::string>());
  EXPECT_EQ("baz=baz2", form["baz"].get<std::string>());
}

TEST(CurlRequestTest, Handle404) {
  storage::internal::CurlRequest request(HttpBinEndpoint() + "/status/404");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  request.PrepareRequest(std::string{});
  auto response = request.MakeRequest();
  EXPECT_EQ(404, response.status_code);
}

/// @test Verify the payload for error status is included in the return value.
TEST(CurlRequestTest, HandleTeapot) {
  storage::internal::CurlRequest request(HttpBinEndpoint() + "/status/418");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  request.PrepareRequest(std::string{});
  auto response = request.MakeRequest();
  EXPECT_EQ(418, response.status_code);
  EXPECT_THAT(response.payload, HasSubstr("[ teapot ]"));
}

/// @test Verify the response includes the header values.
TEST(CurlRequestTest, CheckResponseHeaders) {
  // Test that headers are parsed correctly. We send capitalized headers
  // because some versions of httpbin capitalize and others do not, in real
  // code (as opposed to a test), we should search for headers in a
  // case-insensitive manner, but that is not the purpose of this test.
  storage::internal::CurlRequest request(HttpBinEndpoint() +
                                         "/response-headers"
                                         "?X-Test-Foo=bar"
                                         "&X-Test-Empty");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  request.PrepareRequest(std::string{});
  auto response = request.MakeRequest();
  EXPECT_EQ(200, response.status_code);
  EXPECT_EQ(1U, response.headers.count("X-Test-Empty"));
  EXPECT_EQ("", response.headers.find("X-Test-Empty")->second);
  EXPECT_LE(1U, response.headers.count("X-Test-Foo"));
  EXPECT_EQ("bar", response.headers.find("X-Test-Foo")->second);
}

/// @test Verify that the Projection parameter is included if set.
TEST(CurlRequestTest, WellKnownQueryParameters_Projection) {
  storage::internal::CurlRequest request(HttpBinEndpoint() + "/get");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  storage::WellKnownParameters parameters;
  parameters.Apply(storage::Projection("full"));
  request.AddWellKnownParameters(parameters);

  request.PrepareRequest(std::string{});
  auto response = request.MakeRequest();
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
  storage::internal::CurlRequest request(HttpBinEndpoint() + "/get");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  storage::WellKnownParameters parameters;
  parameters.Apply(storage::UserProject("a-project"));
  request.AddWellKnownParameters(parameters);

  request.PrepareRequest(std::string{});
  auto response = request.MakeRequest();
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
  storage::internal::CurlRequest request(HttpBinEndpoint() + "/get");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  storage::WellKnownParameters parameters;
  parameters.Apply(storage::IfGenerationMatch(42));
  request.AddWellKnownParameters(parameters);

  request.PrepareRequest(std::string{});
  auto response = request.MakeRequest();
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
  storage::internal::CurlRequest request(HttpBinEndpoint() + "/get");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  storage::WellKnownParameters parameters;
  parameters.Apply(storage::IfGenerationNotMatch(42));
  request.AddWellKnownParameters(parameters);

  request.PrepareRequest(std::string{});
  auto response = request.MakeRequest();
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

/// @test Verify that the IfMetaGenerationMatch parameter is included if set.
TEST(CurlRequestTest, WellKnownQueryParameters_IfMetaGenerationMatch) {
  storage::internal::CurlRequest request(HttpBinEndpoint() + "/get");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  storage::WellKnownParameters parameters;
  parameters.Apply(storage::IfMetaGenerationMatch(42));
  request.AddWellKnownParameters(parameters);

  request.PrepareRequest(std::string{});
  auto response = request.MakeRequest();
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

/// @test Verify that the IfMetaGenerationNotMatch parameter is included if set.
TEST(CurlRequestTest, WellKnownQueryParameters_IfMetaGenerationNotMatch) {
  storage::internal::CurlRequest request(HttpBinEndpoint() + "/get");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  storage::WellKnownParameters parameters;
  parameters.Apply(storage::IfMetaGenerationNotMatch(42));
  request.AddWellKnownParameters(parameters);

  request.PrepareRequest(std::string{});
  auto response = request.MakeRequest();
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
  storage::internal::CurlRequest request(HttpBinEndpoint() + "/get");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  storage::WellKnownParameters parameters;
  parameters.Apply(storage::UserProject("user-project-id"));
  parameters.Apply(storage::IfMetaGenerationMatch(7));
  parameters.Apply(storage::IfGenerationNotMatch(42));
  request.AddWellKnownParameters(parameters);

  request.PrepareRequest(std::string{});
  auto response = request.MakeRequest();
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

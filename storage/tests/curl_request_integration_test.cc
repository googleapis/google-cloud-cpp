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

#include "storage/client/internal/curl_request.h"
#include <gtest/gtest.h>
#include <vector>

namespace nl = storage::internal::nl;

TEST(CurlRequestTest, SimpleGET) {
  // TODO(#542) - use a local server to make tests more hermetic.
  storage::internal::CurlRequest request("https://nghttp2.org/httpbin/get");
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
  // TODO(#542) - use a local server to make tests more hermetic.
  storage::internal::CurlRequest request("https://nghttp2.org/httpbin/get");
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
  // TODO(#542) - use a local server to make tests more hermetic.
  storage::internal::CurlRequest request("https://nghttp2.org/httpbin/post");
  std::vector<std::pair<std::string, std::string>> form_parameters = {
      {"foo", "foo1&foo2 foo3"}, {"bar", "bar1-bar2"}, {"baz", "baz=baz2"},
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
  // TODO(#542) - use a local server to make tests more hermetic.
  storage::internal::CurlRequest request(
      "https://nghttp2.org/httpbin/status/404");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  request.PrepareRequest(std::string{});
  auto response = request.MakeRequest();
  EXPECT_EQ(404, response.status_code);
}

/// @test Verify the payload for error status is included in the return value.
TEST(CurlRequestTest, HandleTeapot) {
  // TODO(#542) - use a local server to make tests more hermetic.
  storage::internal::CurlRequest request(
      "https://nghttp2.org/httpbin/status/418");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  request.PrepareRequest(std::string{});
  auto response = request.MakeRequest();
  EXPECT_EQ(418, response.status_code);
  auto pos = response.payload.find("[ teapot ]");
  EXPECT_NE(std::string::npos, pos);
}

/// @test Verify the response includes the header values.
TEST(CurlRequestTest, CheckResponseHeaders) {
  // TODO(#542) - use a local server to make tests more hermetic.
  storage::internal::CurlRequest request(
      "https://nghttp2.org/httpbin/response-headers"
      "?x-test-foo=bar"
      "&x-test-foo=baz"
      "&X-Test-Empty");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  request.PrepareRequest(std::string{});
  auto response = request.MakeRequest();
  EXPECT_EQ(200, response.status_code);
  EXPECT_EQ(2U, response.headers.count("X-Test-Foo"));
  EXPECT_EQ("bar", response.headers.find("X-Test-Foo")->second);
  EXPECT_EQ(1U, response.headers.count("X-Test-Empty"));
  EXPECT_EQ("", response.headers.find("X-Test-Empty")->second);
}

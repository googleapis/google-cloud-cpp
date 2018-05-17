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
  storage::internal::CurlRequest request("https://nghttp2.org/httpbin/get");
  request.AddQueryParameter("foo", "foo1&&&foo2");
  request.AddQueryParameter("bar", "bar1==bar2=");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  request.PrepareRequest(std::string{});
  auto response_text = request.MakeRequest();
  nl::json response = nl::json::parse(response_text);
  nl::json args = response["args"];
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
  storage::internal::CurlRequest request("https://nghttp2.org/httpbin/get");
  request.AddQueryParameter("foo", "foo1&&&foo2");
  request.AddQueryParameter("bar", "bar1==bar2=");
  request.AddHeader("Accept: application/json");
  request.AddHeader("charsets: utf-8");

  request.PrepareRequest(std::string{});

  auto response_text = request.MakeRequest();
  nl::json response = nl::json::parse(response_text);
  nl::json args = response["args"];
  EXPECT_EQ("foo1&&&foo2", args["foo"].get<std::string>());
  EXPECT_EQ("bar1==bar2=", args["bar"].get<std::string>());

  response_text = request.MakeRequest();
  response = nl::json::parse(response_text);
  args = response["args"];
  EXPECT_EQ("foo1&&&foo2", args["foo"].get<std::string>());
  EXPECT_EQ("bar1==bar2=", args["bar"].get<std::string>());
}

TEST(CurlRequestTest, SimpleJSON) {
  storage::internal::CurlRequest request("https://nghttp2.org/httpbin/post");
  request.AddQueryParameter("foo", "bar&baz");
  request.AddQueryParameter("qux", "quux-123");
  request.AddHeader("Accept: application/json");
  request.AddHeader("Content-Type: application/json");
  request.AddHeader("charsets: utf-8");

  request.PrepareRequest(nl::json{{"int", 42}, {"string", "value"}});
  auto response_text = request.MakeRequest();
  nl::json response = nl::json::parse(response_text);
  nl::json args = response["args"];
  EXPECT_EQ("bar&baz", args["foo"].get<std::string>());
  EXPECT_EQ("quux-123", args["qux"].get<std::string>());

  std::string data_text = response["data"].get<std::string>();
  nl::json data = nl::json::parse(data_text);
  EXPECT_EQ(42, data["int"].get<int>());
  EXPECT_EQ("value", data["string"].get<std::string>());
}

TEST(CurlRequestTest, SimplePOST) {
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
  auto response_text = request.MakeRequest();
  nl::json response = nl::json::parse(response_text);
  nl::json form = response["form"];
  EXPECT_EQ("foo1&foo2 foo3", form["foo"].get<std::string>());
  EXPECT_EQ("bar1-bar2", form["bar"].get<std::string>());
  EXPECT_EQ("baz=baz2", form["baz"].get<std::string>());
}

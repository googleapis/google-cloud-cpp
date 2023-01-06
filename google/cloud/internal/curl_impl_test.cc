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
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <vector>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;

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

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

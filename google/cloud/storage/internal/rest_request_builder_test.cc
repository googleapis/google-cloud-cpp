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

#include "google/cloud/storage/internal/rest_request_builder.h"
#include "google/cloud/storage/internal/generic_request.h"
#include "google/cloud/storage/well_known_headers.h"
#include "google/cloud/storage/well_known_parameters.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using google::cloud::rest_internal::RestRequest;
using ::testing::Eq;
using ::testing::UnorderedElementsAre;

TEST(RestRequestBuilderTest, WellKnownParameterRequest) {
  class WellKnownParameterRequest
      : public GenericRequest<WellKnownParameterRequest, Deleted, MaxResults,
                              Prefix, Projection, UserProject> {};

  WellKnownParameterRequest request;
  request.set_option(Deleted(true));
  request.set_option(MaxResults(42));
  request.set_option(Prefix("my_prefix"));
  request.set_option(Projection("my_projection"));
  request.set_option(UserProject("my_project"));

  RestRequestBuilder builder("service/path");
  request.AddOptionsToHttpRequest(builder);
  builder.AddQueryParameter("foo", "bar");

  RestRequest rest_request = std::move(builder).BuildRequest();

  EXPECT_THAT(
      rest_request.parameters(),
      UnorderedElementsAre(
          Eq(std::pair<std::string, std::string>("projection",
                                                 "my_projection")),
          Eq(std::pair<std::string, std::string>("foo", "bar")),
          Eq(std::pair<std::string, std::string>("userProject", "my_project")),
          Eq(std::pair<std::string, std::string>("maxResults", "42")),
          Eq(std::pair<std::string, std::string>("prefix", "my_prefix")),
          Eq(std::pair<std::string, std::string>("deleted", "true"))));
}

TEST(RestRequestBuilderTest, WellKnownHeaderRequest) {
  class WellKnownHeaderRequest
      : public GenericRequest<WellKnownHeaderRequest, ContentType,
                              IfMatchEtag> {};

  WellKnownHeaderRequest request;
  request.set_option(ContentType("application/json"));
  request.set_option(IfMatchEtag("my_etag"));

  RestRequestBuilder builder("service/path");
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("foo", "bar");
  builder.AddHeader("foo", "baz");

  RestRequest rest_request = std::move(builder).BuildRequest();

  EXPECT_THAT(rest_request.headers(),
              UnorderedElementsAre(
                  Eq(std::pair<std::string const, std::vector<std::string>>(
                      "foo", {"bar", "baz"})),
                  Eq(std::pair<std::string const, std::vector<std::string>>(
                      "content-type", {"application/json"})),
                  Eq(std::pair<std::string const, std::vector<std::string>>(
                      "if-match", {"my_etag"}))));
}

TEST(RestRequestBuilderTest, CustomHeaderRequest) {
  class CustomHeaderRequest
      : public GenericRequest<CustomHeaderRequest, CustomHeader> {};

  CustomHeaderRequest request;
  request.set_option(CustomHeader("my_header_key", "my_header_value"));

  RestRequestBuilder builder("service/path");
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("foo", "bar");

  RestRequest rest_request = std::move(builder).BuildRequest();

  EXPECT_THAT(rest_request.headers(),
              UnorderedElementsAre(
                  Eq(std::pair<std::string const, std::vector<std::string>>(
                      "my_header_key", {"my_header_value"})),
                  Eq(std::pair<std::string const, std::vector<std::string>>(
                      "foo", {"bar"}))));
}

TEST(RestRequestBuilderTest, EncryptionKeyHeaderRequest) {
  class EncryptionKeyHeaderRequest
      : public GenericRequest<EncryptionKeyHeaderRequest, EncryptionKey> {};

  EncryptionKeyHeaderRequest request;
  EncryptionKeyData data;
  data.algorithm = "my_algorithm";
  data.key = "my_key";
  data.sha256 = "my_sha256";
  request.set_option(EncryptionKey(data));

  RestRequestBuilder builder("service/path");
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("foo", "bar");

  RestRequest rest_request = std::move(builder).BuildRequest();

  EXPECT_THAT(rest_request.headers(),
              UnorderedElementsAre(
                  Eq(std::pair<std::string const, std::vector<std::string>>(
                      "x-goog-encryption-key-sha256", {"my_sha256"})),
                  Eq(std::pair<std::string const, std::vector<std::string>>(
                      "x-goog-encryption-key", {"my_key"})),
                  Eq(std::pair<std::string const, std::vector<std::string>>(
                      "x-goog-encryption-algorithm", {"my_algorithm"})),
                  Eq(std::pair<std::string const, std::vector<std::string>>(
                      "foo", {"bar"}))));
}

TEST(RestRequestBuilderTest, SourceEncryptionKeyHeaderRequest) {
  class SourceEncryptionKeyHeaderRequest
      : public GenericRequest<SourceEncryptionKeyHeaderRequest,
                              SourceEncryptionKey> {};

  SourceEncryptionKeyHeaderRequest request;
  EncryptionKeyData data;
  data.algorithm = "my_algorithm";
  data.key = "my_key";
  data.sha256 = "my_sha256";
  request.set_option(SourceEncryptionKey(data));

  RestRequestBuilder builder("service/path");
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("foo", "bar");

  RestRequest rest_request = std::move(builder).BuildRequest();

  EXPECT_THAT(rest_request.path(), Eq("service/path"));
  EXPECT_THAT(
      rest_request.headers(),
      UnorderedElementsAre(
          Eq(std::pair<std::string const, std::vector<std::string>>(
              "x-goog-copy-source-encryption-key-sha256", {"my_sha256"})),
          Eq(std::pair<std::string const, std::vector<std::string>>(
              "x-goog-copy-source-encryption-key", {"my_key"})),
          Eq(std::pair<std::string const, std::vector<std::string>>(
              "x-goog-copy-source-encryption-algorithm", {"my_algorithm"})),
          Eq(std::pair<std::string const, std::vector<std::string>>("foo",
                                                                    {"bar"}))));
}

TEST(RestRequestBuilderTest, ComplexOptionRequest) {
  struct TestComplexOption
      : public google::cloud::storage::internal::ComplexOption<
            TestComplexOption, std::string> {
    TestComplexOption() = default;
    explicit TestComplexOption(std::string test)
        : ComplexOption(std::move(test)) {}
    static char const* name() { return "test-complex-option"; }
  };

  class ComplexOptionRequest
      : public GenericRequest<ComplexOptionRequest, TestComplexOption> {};

  ComplexOptionRequest request;
  request.set_option(TestComplexOption("test-complex-option-data"));

  RestRequestBuilder builder("service/path");
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("foo", "bar");

  RestRequest rest_request = std::move(builder).BuildRequest();

  EXPECT_THAT(rest_request.path(), Eq("service/path"));
  EXPECT_THAT(rest_request.headers(),
              UnorderedElementsAre(
                  Eq(std::pair<std::string const, std::vector<std::string>>(
                      "foo", {"bar"}))));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

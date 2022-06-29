// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_VALIDATE_METADATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_VALIDATE_METADATA_H

#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <google/api/annotations.pb.h>
#include <google/protobuf/descriptor.h>
#include <gmock/gmock.h>
#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/grpcpp.h>
#include <map>
#include <regex>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

/**
 * We do not use `::testing::MatchesRegex` because our Windows builds use a
 * googletest built with `GTEST_USES_SIMPLE_RE`, instead of
 * `GTEST_USES_POSIX_RE`.
 */
MATCHER_P(MatchesMetadataRegex, pattern,
          "matches the pattern: \"" + pattern + "\"") {
  // Translate the pattern into a form that can be used by std::regex
  std::regex regex(absl::StrReplaceAll(pattern, {{"*", "[^/]+"}}));
  return std::regex_match(arg, regex);
}

std::map<std::string, std::string> ExtractMDFromHeader(std::string header);

std::map<std::string, std::string> FromHttpRule(
    google::api::HttpRule const& http,
    absl::optional<std::string> const& resource_name);

template <typename Request>
std::map<std::string, std::string> ExtractRoutingHeaders(
    std::string const& method, Request const&,
    absl::optional<std::string> const& resource_name) {
  auto const* method_desc =
      google::protobuf::DescriptorPool::generated_pool()->FindMethodByName(
          method);
  using ::testing::NotNull;
  EXPECT_THAT(method_desc, NotNull()) << "Method " + method + " is unknown.";
  if (!method_desc) return {};
  auto options = method_desc->options();
  // TODO(#9373): Handle `google::api::routing` extension.
  if (options.HasExtension(google::api::http)) {
    auto const& http = options.GetExtension(google::api::http);
    return FromHttpRule(http, resource_name);
  }
  return {};
}

/**
 * Keep the test required to test metadata contents in a grpc::Context object.
 *
 * Our libraries need to set a number of metadata attributes in the
 * `grpc::ClientContext` objects used to make RPCs.  Naturally, we want to write
 * tests for the functions that set this metadata, but `grpc::ClientContext`
 * does not have any APIs to examine the metadata previously set.
 *
 * However, we can make a call to a *local* gRPC server, and then examine the
 * metadata on this server. Unfortunately, initializing these servers can be
 * slow, particularly on Windows.  It is worthwhile to cache the state and the
 * server in a fixture, so the tests run faster.
 */
class ValidateMetadataFixture {
 public:
  ValidateMetadataFixture();
  ~ValidateMetadataFixture();

  /**
   * GetMetadata from `ClientContext`.
   *
   * @note A `grpc::ClientContext` can be used in only one gRPC. The caller
   *   cannot reuse @p context for other RPCs or other calls to this function.
   */
  std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext& context);

  /**
   * Verify that the metadata in the context is appropriate for a gRPC method.
   *
   * `ClientContext` should instruct gRPC to set a `x-goog-request-params` HTTP
   * header with a value dictated by a `google.api.http` option in the gRPC
   * service specification. This function checks if the header is set and
   * whether it has a valid value.
   *
   * @note A `grpc::ClientContext` can be used in only one gRPC. The caller
   *   cannot reuse @p context for other RPCs or other calls to this function.
   *
   * @param context the context to validate
   * @param method a gRPC method which which this context will be passed to
   * @param api_client_header expected value for the x-goog-api-client metadata
   *     header.
   * @param resource_prefix_header if specified, this is the expected value for
   *     the google-cloud-resource-prefix metadata header.
   *
   * @return an OK status if the `context` is properly set up
   */
  template <typename Request>
  void IsContextMDValid(
      grpc::ClientContext& context, std::string const& method,
      Request const& request, std::string const& api_client_header,
      absl::optional<std::string> const& resource_name = {},
      absl::optional<std::string> const& resource_prefix_header = {}) {
    using ::testing::Contains;
    using ::testing::Pair;

    auto headers = GetMetadata(context);

    // Check x-goog-api-client first, because it should always be present.
    EXPECT_THAT(headers,
                Contains(Pair("x-goog-api-client", api_client_header)));

    if (resource_prefix_header) {
      EXPECT_THAT(headers, Contains(Pair("google-cloud-resource-prefix",
                                         *resource_prefix_header)));
    }

    // Extract the metadata from `x-goog-request-params` header in context.
    auto param_header = headers.equal_range("x-goog-request-params");
    auto dist = std::distance(param_header.first, param_header.second);
    EXPECT_LE(dist, 1U) << "Multiple x-goog-request-params headers found";
    auto actual = dist == 0 ? std::map<std::string, std::string>{}
                            : ExtractMDFromHeader(param_header.first->second);

    // Extract expectations on `x-goog-request-params` from the
    // `google.api.http` annotation on the specified method.
    auto expected = ExtractRoutingHeaders(method, request, resource_name);

    // Check if the metadata in the context satisfied the expectations.
    for (auto const& param : expected) {
      EXPECT_THAT(actual, Contains(Pair(param.first,
                                        MatchesMetadataRegex(param.second))));
    }
  }

 private:
  grpc::CompletionQueue cli_cq_;
  grpc::AsyncGenericService generic_service_;
  std::unique_ptr<grpc::ServerCompletionQueue> srv_cq_;
  std::unique_ptr<grpc::Server> server_;
};

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_VALIDATE_METADATA_H

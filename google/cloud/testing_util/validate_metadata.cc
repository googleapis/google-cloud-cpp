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

#include "google/cloud/testing_util/validate_metadata.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/internal/url_encode.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_split.h"
#include <google/api/annotations.pb.h>
#include <google/api/routing.pb.h>
#include <google/protobuf/descriptor.h>
#include <gmock/gmock.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/generic/generic_stub.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <deque>
#include <iterator>
#include <regex>
#include <type_traits>

// Undefine a Windows macro, which conflicts with
// `protobuf::Reflection::GetMessage`
#undef GetMessage

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

namespace {

using ::google::protobuf::DescriptorPool;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::Pair;
using ::testing::UnorderedElementsAreArray;

using RoutingHeaders = std::map<std::string, std::string>;

/**
 * Check if the `header` is of "foo=bar&baz=rab&..." and if it is, return a
 * `map` containing `"foo"->"bar", "baz"->"rab"`.
 */
RoutingHeaders ExtractMDFromHeader(std::string header) {
  std::map<std::string, std::string> res;
  std::regex pair_re("[^&]+");
  for (std::sregex_iterator i =
           std::sregex_iterator(header.begin(), header.end(), pair_re);
       i != std::sregex_iterator(); ++i) {
    std::regex assign_re("([^=]+)=([^=]+)");
    std::smatch match_res;
    std::string s = i->str();
    bool const matched = std::regex_match(s, match_res, assign_re);
    EXPECT_TRUE(matched)
        << "Bad header format. The header should be a series of \"a=b\" "
           "delimited with \"&\", but is \"" +
               s + "\"";
    if (!matched) continue;
    bool const inserted =
        res.insert(std::make_pair(match_res[1].str(), match_res[2].str()))
            .second;
    EXPECT_TRUE(inserted) << "Param " + match_res[1].str() +
                                 " is listed more than once";
  }
  return res;
}

/**
 * Verify that the string contains no reserved characters, other than '%'.
 *
 * See: https://datatracker.ietf.org/doc/html/rfc3986#section-2.1
 *
 * Note that it will match something like "%xy", which is not URL encoded. A
 * more accurate name might be: `IsntObviouslyNotUrlEncoded`. The important
 * thing is that the match will fail if it encounters a '/', which is found in
 * almost all of these routing values.
 */
MATCHER(IsUrlEncoded, "") {
  std::regex regex(R"re([A-Za-z0-9%_.~-]*)re");
  return std::regex_match(arg, regex);
}

/**
 * We do not use `::testing::MatchesRegex` because our Windows builds use a
 * googletest built with `GTEST_USES_SIMPLE_RE`, instead of
 * `GTEST_USES_POSIX_RE`.
 */
MATCHER_P(MatchesGlob, glob, "matches the glob: \"" + glob + "\"") {
  // Translate the `glob` into a regex pattern.
  auto matcher = absl::StrReplaceAll(glob, {{"*", "[^/]+"}});
  std::regex regex(matcher);
  // Decode the `arg` before trying to match it.
  return std::regex_match(internal::UrlDecode(arg), regex);
}

// This method is recursive because dbolduc could not figure out the iterative
// solution.
std::string GetField(  // NOLINT(misc-no-recursion)
    std::deque<std::string> fields,
    google::protobuf::Descriptor const* input_type,
    google::protobuf::Message const& msg) {
  if (fields.empty()) {
    GCP_LOG(FATAL) << "Empty field name defined in RoutingRule.";
  }
  auto const* fd = input_type->FindFieldByName(fields.front());
  fields.pop_front();
  if (fields.empty()) return msg.GetReflection()->GetString(msg, fd);
  auto const& sub_msg = msg.GetReflection()->GetMessage(msg, fd);
  return GetField(fields, fd->message_type(), sub_msg);
}

/**
 * Parse the `RoutingRule` proto as described in the proto comments:
 * https://github.com/googleapis/googleapis/blob/master/google/api/routing.proto
 *
 * We loop over the repeated `routing_parameters` field. For each one we attempt
 * to match and extract a routing key-value pair.
 *
 * We may end up matching the same key multiple times. If this happens, we
 * overwrite the current value in the map, because the "last match wins".
 */
RoutingHeaders FromRoutingRule(google::api::RoutingRule const& routing,
                               google::protobuf::MethodDescriptor const* method,
                               google::protobuf::Message const& request) {
  RoutingHeaders headers;
  for (auto const& rp : routing.routing_parameters()) {
    auto const& path_template = rp.path_template();
    // Some fields may look like: "nested1.nested2.value", where `nested1` and
    // `nested2` are generic Messages, and `value` is the string field we are to
    // match against. We must iterate over the nested messages to get to the
    // string value.
    std::deque<std::string> fields = absl::StrSplit(rp.field(), '.');
    auto const& field = GetField(fields, method->input_type(), request);

    // We skip empty fields.
    if (field.empty()) continue;
    // If the path_template is empty, we use the field's name as the routing
    // param key, and we match the entire value of the field.
    if (path_template.empty()) {
      headers[rp.field()] = field;
      continue;
    }
    // First we parse the path_template field to extract the routing param key
    static std::regex const kPatternRegex(R"((.*)\{(.*)=(.*)\}(.*))");
    std::smatch match;
    if (!std::regex_match(path_template, match, kPatternRegex)) {
      GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__ << ": "
                     << "RoutingParameters path template is malformed: "
                     << path_template;
    }
    auto pattern =
        absl::StrCat(match[1].str(), "(", match[3].str(), ")", match[4].str());
    pattern = absl::StrReplaceAll(pattern, {{"**", ".*"}, {"*", "[^/]+"}});
    auto param = match[2].str();
    // Then we parse the field in the given request to see if it matches the
    // pattern we expect.
    if (std::regex_match(field, match, std::regex{pattern})) {
      headers[std::move(param)] = match[1].str();
    }
  }
  return headers;
}

/**
 * Given a `method`, extract its `google.api.http` option and parse it.
 *
 * The expected format of the option is
 * `something{foo=bar}something_else{baz=rab}`. For such a content, a `map`
 * containing `"foo"->"bar", "baz"->"rab"` is returned.
 */
RoutingHeaders FromHttpRule(google::api::HttpRule const& http,
                            absl::optional<std::string> const& resource_name) {
  RoutingHeaders headers;
  std::string pattern;
  if (!http.get().empty()) {
    pattern = http.get();
  } else if (!http.put().empty()) {
    pattern = http.put();
  } else if (!http.post().empty()) {
    pattern = http.post();
  } else if ((http.additional_bindings_size() > 0) && resource_name &&
             !(http.additional_bindings(0).post().empty())) {
    pattern = http.additional_bindings(0).post();
  } else if (!http.delete_().empty()) {
    pattern = http.delete_();
  } else if (!http.patch().empty()) {
    pattern = http.patch();
  } else if (http.has_custom()) {
    pattern = http.custom().path();
  }

  EXPECT_THAT(pattern, Not(IsEmpty()))
      << "Method has an http option with an empty pattern.";
  if (pattern.empty()) return headers;

  std::regex subst_re("\\{([^{}=]+)=([^{}=]+)\\}");
  for (std::sregex_iterator i =
           std::sregex_iterator(pattern.begin(), pattern.end(), subst_re);
       i != std::sregex_iterator(); ++i) {
    std::string const& param = (*i)[1].str();
    std::string const& expected_pattern = (*i)[2].str();
    headers.insert(std::make_pair(param, expected_pattern));
  }
  return headers;
}

RoutingHeaders ExtractRoutingHeaders(
    google::protobuf::MethodDescriptor const* method,
    google::protobuf::Message const& request,
    absl::optional<std::string> const& resource_name) {
  auto options = method->options();
  if (options.HasExtension(google::api::routing)) {
    auto const& routing = options.GetExtension(google::api::routing);
    return FromRoutingRule(routing, method, request);
  }
  if (options.HasExtension(google::api::http)) {
    auto const& http = options.GetExtension(google::api::http);
    return FromHttpRule(http, resource_name);
  }
  return {};
}

}  // namespace

/**
 * Set server metadata on a `ClientContext`.
 *
 * @note A `grpc::ClientContext` can be used in only one gRPC. The caller
 *   cannot reuse @p context for other RPCs or other calls to this function.
 */
void SetServerMetadata(grpc::ClientContext& context,
                       RpcMetadata const& server_metadata,
                       bool is_initial_metadata_ready) {
  ValidateMetadataFixture f;
  f.SetServerMetadata(context, server_metadata, is_initial_metadata_ready);
}

ValidateMetadataFixture::ValidateMetadataFixture() {
  // Start the generic server.
  grpc::ServerBuilder builder;
  builder.RegisterAsyncGenericService(&generic_service_);
  srv_cq_ = builder.AddCompletionQueue();
  server_ = builder.BuildAndStart();
}

ValidateMetadataFixture::~ValidateMetadataFixture() {
  // Shut everything down.
  server_->Shutdown(std::chrono::system_clock::now());
  srv_cq_->Shutdown();
  cli_cq_.Shutdown();

  // Drain completion queues.
  void* placeholder;
  bool ok;
  while (srv_cq_->Next(&placeholder, &ok)) continue;
  while (cli_cq_.Next(&placeholder, &ok)) continue;
}

std::multimap<std::string, std::string> ValidateMetadataFixture::GetMetadata(
    grpc::ClientContext& client_context) {
  // Set the deadline to far in the future. If the deadline is in the past,
  // gRPC doesn't send the initial metadata at all (which makes sense, given
  // that the context is already expired). The `context` is destroyed by this
  // function anyway, so we're not making things worse by changing the
  // deadline.
  client_context.set_deadline(std::chrono::system_clock::now() +
                              std::chrono::hours(24));

  grpc::GenericServerContext server_context;
  ExchangeMetadata(client_context, server_context);

  // Now we've got the data - save it before cleaning up.
  std::multimap<std::string, std::string> res;
  auto const& cli_md = server_context.client_metadata();
  std::transform(cli_md.begin(), cli_md.end(), std::inserter(res, res.begin()),
                 [](std::pair<grpc::string_ref, grpc::string_ref> const& md) {
                   return std::make_pair(
                       std::string(md.first.data(), md.first.length()),
                       std::string(md.second.data(), md.second.length()));
                 });

  return res;
}

// Older versions of gRPC do not provide `ExperimentalGetAuthority()`. In that
// case just return `absl::nullopt`, to indicate that the test cannot run.
template <typename T, typename AlwaysVoid = void>
struct GetAuthorityImpl {
  absl::optional<std::string> Get(T& /*sc*/) { return absl::nullopt; }
};

template <typename T>
struct GetAuthorityImpl<
    T, absl::void_t<decltype(std::declval<T>().ExperimentalGetAuthority())>> {
  absl::optional<std::string> Get(T& sc) {
    auto result = sc.ExperimentalGetAuthority();
    return std::string(result.data(), result.size());
  }
};

absl::optional<std::string> ValidateMetadataFixture::GetAuthority(
    grpc::ClientContext& client_context) {
  // Set the deadline to far in the future. If the deadline is in the past,
  // gRPC doesn't send the initial metadata at all (which makes sense, given
  // that the context is already expired). The `context` is destroyed by this
  // function anyway, so we're not making things worse by changing the
  // deadline.
  client_context.set_deadline(std::chrono::system_clock::now() +
                              std::chrono::hours(24));

  grpc::GenericServerContext server_context;
  ExchangeMetadata(client_context, server_context);

  return GetAuthorityImpl<grpc::GenericServerContext>{}.Get(server_context);
}

void ValidateMetadataFixture::SetServerMetadata(
    grpc::ClientContext& client_context, RpcMetadata const& server_metadata,
    bool is_initial_metadata_ready) {
  // Create a server context with the given server metadata.
  grpc::GenericServerContext server_context;
  if (is_initial_metadata_ready) {
    for (auto const& kv : server_metadata.headers) {
      server_context.AddInitialMetadata(kv.first, kv.second);
    }
  }
  for (auto const& kv : server_metadata.trailers) {
    server_context.AddTrailingMetadata(kv.first, kv.second);
  }

  ExchangeMetadata(client_context, server_context);
}

/**
 * We use reflection to extract the `google.api.http` option from the given
 * `method`. We then parse it and check whether the contents of the
 * `x-goog-request-params` header in `context` set all the parameters listed in
 * the curly braces.
 */
void ValidateMetadataFixture::IsContextMDValid(
    grpc::ClientContext& context, std::string const& method_name,
    google::protobuf::Message const& request,
    std::string const& api_client_header,
    absl::optional<std::string> const& resource_name,
    absl::optional<std::string> const& resource_prefix_header) {
  auto headers = GetMetadata(context);

  // Check x-goog-api-client first, because it should always be present.
  EXPECT_THAT(headers, Contains(Pair("x-goog-api-client", api_client_header)));

  if (resource_prefix_header) {
    EXPECT_THAT(headers, Contains(Pair("google-cloud-resource-prefix",
                                       *resource_prefix_header)));
  }

  // Extract the metadata from `x-goog-request-params` header in context.
  auto param_header = headers.equal_range("x-goog-request-params");
  auto dist = std::distance(param_header.first, param_header.second);
  EXPECT_LE(dist, 1U) << "Multiple x-goog-request-params headers found";
  auto actual = dist == 0 ? RoutingHeaders{}
                          : ExtractMDFromHeader(param_header.first->second);

  auto const* method =
      DescriptorPool::generated_pool()->FindMethodByName(method_name);
  if (method == nullptr) {
    GCP_LOG(INFO) << "`x-goog-request-params` header not verified for "
                  << method_name << ", because it is unknown.";
    return;
  }

  // Extract expectations on `x-goog-request-params` from the
  // `google.api.routing` or `google.api.http` annotation on the specified
  // method.
  auto expected = ExtractRoutingHeaders(method, request, resource_name);

  // Do not verify routing parameters for streaming writes, because it is not
  // well defined.
  if (method->client_streaming()) {
    GCP_LOG(INFO) << "`x-goog-request-params` header not verified for "
                  << method_name << ", because it is a streaming write.";
    return;
  }

  // Check if the metadata in the context satisfied the expectations.
  std::vector<Matcher<std::pair<std::string, std::string>>> matchers;
  matchers.reserve(expected.size());
  for (auto const& param : expected) {
    matchers.push_back(
        Pair(param.first, AllOf(IsUrlEncoded(), MatchesGlob(param.second))));
  }
  EXPECT_THAT(actual, UnorderedElementsAreArray(matchers));
}

void ValidateMetadataFixture::ExchangeMetadata(
    grpc::ClientContext& client_context,
    grpc::GenericServerContext& server_context) {
  void* tag;
  bool ok;

  // Create an in-process stub to make client calls with.
  grpc::GenericStub generic_stub(
      server_->InProcessChannel(grpc::ChannelArguments()));

  // Start a call, the contents of which, do not matter.
  auto cli_stream =
      generic_stub.PrepareCall(&client_context, "made_up_method", &cli_cq_);
  int call_tag;
  cli_stream->StartCall(&call_tag);
  cli_cq_.Next(&tag, &ok);
  ASSERT_TRUE(ok && tag == &call_tag) << "stub.PrepareCall() failed.";

  grpc::GenericServerAsyncReaderWriter reader_writer(&server_context);

  // Process the request.
  int request_tag;
  generic_service_.RequestCall(&server_context, &reader_writer, srv_cq_.get(),
                               srv_cq_.get(), &request_tag);
  srv_cq_->Next(&tag, &ok);
  ASSERT_TRUE(ok && tag == &request_tag) << "service.RequestCall() failed.";

  // Finish the stream on the server-side.
  int srv_finish_tag;
  reader_writer.Finish(
      grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "unimplemented"),
      &srv_finish_tag);
  srv_cq_->Next(&tag, &ok);
  ASSERT_TRUE(ok && tag == &srv_finish_tag) << "reader_writer.Finish() failed.";

  // Finish the stream on the client-side.
  grpc::Status status;
  int cli_finish_tag;
  cli_stream->Finish(&status, &cli_finish_tag);
  cli_cq_.Next(&tag, &ok);
  ASSERT_TRUE(ok && tag == &cli_finish_tag) << "stream.Finish() failed.";
}

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

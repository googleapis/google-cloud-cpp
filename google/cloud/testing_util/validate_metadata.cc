// Copyright 2019 Google LLC
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

#include "google/cloud/testing_util/validate_metadata.h"
#include "google/cloud/status_or.h"
#include <google/api/annotations.pb.h>
#include <google/protobuf/descriptor.h>
#include <grpcpp/channel.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/generic/generic_stub.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <regex>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

namespace {

/**
 * GetMetadata from `ClientContext`.
 *
 * `ClientContext` doesn't give access to the metadata, but `ServerContext`
 * does. In order to transform the `ClientContext` into `ServerContext`
 * we spin up a server and a client and send some garbage with this context.
 */
std::multimap<std::string, std::string> GetMetadata(
    grpc::ClientContext& context) {
  // Set the deadline to far in the future. If the deadline is in the past, gRPC
  // doesn't send the initial metadata at all (which makes sense, given that the
  // context is already expired). The `context` is destroyed by this function
  // anyway, so we're not making things worse by changing the deadline.
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::hours(24));

  // Start the generic server.
  grpc::ServerBuilder builder;
  grpc::AsyncGenericService generic_service;
  builder.RegisterAsyncGenericService(&generic_service);
  auto srv_cq = builder.AddCompletionQueue();
  auto server = builder.BuildAndStart();

  // Send some garbage with the supplied context.
  grpc::GenericStub generic_stub(
      server->InProcessChannel(grpc::ChannelArguments()));
  grpc::CompletionQueue cli_cq;
  auto cli_stream =
      generic_stub.PrepareCall(&context, "made_up_method", &cli_cq);
  cli_stream->StartCall(nullptr);
  bool ok;
  void* placeholder;
  cli_cq.Next(&placeholder, &ok);  // actually start the client call

  // Receive the garbage with the supplied context.
  grpc::GenericServerContext server_context;
  grpc::GenericServerAsyncReaderWriter reader_writer(&server_context);
  generic_service.RequestCall(&server_context, &reader_writer, srv_cq.get(),
                              srv_cq.get(), nullptr);
  srv_cq->Next(&placeholder, &ok);  // actually receive the data

  // Now we've got the data - save it before cleaning up.
  std::multimap<std::string, std::string> res;
  auto const& cli_md = server_context.client_metadata();
  std::transform(cli_md.begin(), cli_md.end(), std::inserter(res, res.begin()),
                 [](std::pair<grpc::string_ref, grpc::string_ref> const& md) {
                   return std::make_pair(
                       std::string(md.first.data(), md.first.length()),
                       std::string(md.second.data(), md.second.length()));
                 });

  // Shut everything down.
  server->Shutdown(std::chrono::system_clock::now());
  srv_cq->Shutdown();
  cli_cq.Shutdown();
  // Drain completion queues.
  while (srv_cq->Next(&placeholder, &ok))
    ;
  while (cli_cq.Next(&placeholder, &ok))
    ;

  return res;
}

/**
 * Check if the `header` is of "foo=bar&baz=rab&..." and if it is, return a
 * `map` containing `"foo"->"bar", "baz"->"rab"`.
 */
StatusOr<std::map<std::string, std::string> > ExtractMDFromHeader(
    std::string header) {
  std::map<std::string, std::string> res;
  std::regex pair_re("[^&]+");
  for (std::sregex_iterator i =
           std::sregex_iterator(header.begin(), header.end(), pair_re);
       i != std::sregex_iterator(); ++i) {
    std::regex assign_re("([^=]+)=([^=]+)");
    std::smatch match_res;
    std::string s = i->str();
    bool const matched = std::regex_match(s, match_res, assign_re);
    if (!matched) {
      return Status(
          StatusCode::kInvalidArgument,
          "Bad header format. The header should be a series of \"a=b\" "
          "delimited with \"&\", but is \"" +
              s + "\"");
    }
    bool const inserted =
        res.insert(std::make_pair(match_res[1].str(), match_res[2].str()))
            .second;
    if (!inserted) {
      return Status(
          StatusCode::kInvalidArgument,
          "Param " + match_res[1].str() + " is listed more then once");
    }
  }
  return res;
}

StatusOr<std::map<std::string, std::string> > ExtractMDFromHeaders(
    std::multimap<std::string, std::string> const& headers) {
  auto param_header = headers.equal_range("x-goog-request-params");
  if (param_header.first == param_header.second) {
    return Status(StatusCode::kInvalidArgument, "Expected header not found");
  }
  if (std::distance(param_header.first, param_header.second) > 1U) {
    return Status(StatusCode::kInvalidArgument, "Multiple headers found");
  }
  return ExtractMDFromHeader(param_header.first->second);
}

/// A poor man's check if a value matches a glob used in URL patterns.
bool ValueMatchesPattern(std::string const& val, std::string const& pattern) {
  std::string regexified_pattern =
      regex_replace(pattern, std::regex("\\*"), std::string("[^/]+"));
  return std::regex_match(val, std::regex(regexified_pattern));
}

/**
 * Given a `method`, extract its `google.api.http` option and parse it.
 *
 * The expected format of the option is
 * `something{foo=bar}something_else{baz=rab}`. For such a content, a `map`
 * containing `"foo"->"bar", "baz"->"rab"` is returned.
 */
StatusOr<std::map<std::string, std::string> > ExtractParamsFromMethod(
    std::string const& method,
    absl::optional<std::string> const& resource_name) {
  auto const* method_desc =
      google::protobuf::DescriptorPool::generated_pool()->FindMethodByName(
          method);

  if (method_desc == nullptr) {
    return Status(StatusCode::kInvalidArgument,
                  "Method " + method + " is unknown.");
  }
  auto options = method_desc->options();
  if (!options.HasExtension(google::api::http)) {
    return std::map<std::string, std::string>{};
  }
  auto const& http = options.GetExtension(google::api::http);
  std::string pattern;
  if (!http.get().empty()) {
    pattern = http.get();
  }
  if (!http.put().empty()) {
    pattern = http.put();
  }
  if (!http.post().empty()) {
    pattern = http.post();
  }
  if ((http.additional_bindings_size() > 0) && resource_name &&
      !(http.additional_bindings(0).post().empty())) {
    pattern = http.additional_bindings(0).post();
  }
  if (!http.delete_().empty()) {
    pattern = http.delete_();
  }
  if (!http.patch().empty()) {
    pattern = http.patch();
  }
  if (http.has_custom()) {
    pattern = http.custom().path();
  }

  if (pattern.empty()) {
    return Status(
        StatusCode::kInvalidArgument,
        "Method " + method + " has a http option with an empty pattern.");
  }

  std::regex subst_re("\\{([^{}=]+)=([^{}=]+)\\}");
  std::map<std::string, std::string> res;
  for (std::sregex_iterator i =
           std::sregex_iterator(pattern.begin(), pattern.end(), subst_re);
       i != std::sregex_iterator(); ++i) {
    std::string const& param = (*i)[1].str();
    std::string const& expected_pattern = (*i)[2].str();
    res.insert(std::make_pair(param, expected_pattern));
  }
  return res;
}

}  // namespace

/**
 * We use reflection to extract the `google.api.http` option from the given
 * `method`. We then parse it and check whether the contents of the
 * `x-goog-request-params` header in `context` set all the parameters listed in
 * the curly braces.
 */
Status IsContextMDValid(
    grpc::ClientContext& context, std::string const& method,
    std::string const& api_client_header,
    absl::optional<std::string> const& resource_name,
    absl::optional<std::string> const& resource_prefix_header) {
  auto headers = GetMetadata(context);

  // Extract the metadata from `x-goog-request-params` header in context.
  auto md = ExtractMDFromHeaders(headers);
  if (!md) {
    return md.status();
  }

  // Extract expectations on `x-goog-request-params` from the `google.api.http`
  // annotation on the specified method.
  auto params = ExtractParamsFromMethod(method, resource_name);
  if (!params) {
    return params.status();
  }
  // Check if the metadata in the context satisfied the expectations.
  for (auto const& param_pattern : *params) {
    auto const& param = param_pattern.first;
    auto const& expected_pattern = param_pattern.second;
    auto found_it = md->find(param);
    if (found_it == md->end()) {
      return Status(StatusCode::kInvalidArgument,
                    "Expected param \"" + param + "\" not found in metadata");
    }
    if (!ValueMatchesPattern(found_it->second, expected_pattern)) {
      return Status(
          StatusCode::kInvalidArgument,
          "Expected param \"" + param + "\" found, but its value (\"" +
              // NOLINTNEXTLINE(performance-inefficient-string-concatenation)
              found_it->second + "\") does not satisfy the pattern (\"" +
              expected_pattern + "\").");
    }
  }

  auto found_api_client_header = headers.find("x-goog-api-client");
  if (found_api_client_header == headers.end()) {
    return Status(StatusCode::kInvalidArgument,
                  "Expected x-goog-api-client metadata");
  }
  if (found_api_client_header->second != api_client_header) {
    return Status(StatusCode::kInvalidArgument,
                  "Expected x-goog-api-client to be " + api_client_header +
                      ", was " + found_api_client_header->second);
  }
  if (resource_prefix_header) {
    std::string const header = "google-cloud-resource-prefix";
    auto it = headers.find(header);
    if (it == headers.end()) {
      return Status(StatusCode::kInvalidArgument, header + " not found");
    }
    if (it->second != *resource_prefix_header) {
      return Status(StatusCode::kInvalidArgument,
                    header + " expected to be " + *resource_prefix_header +
                        ", but was " + it->second);
    }
  }

  return Status();
}

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

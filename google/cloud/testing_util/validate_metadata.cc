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
#include "google/cloud/status_or.h"
#include <grpcpp/completion_queue.h>
#include <grpcpp/generic/generic_stub.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <iterator>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

using ::testing::IsEmpty;
using ::testing::Not;

/**
 * Check if the `header` is of "foo=bar&baz=rab&..." and if it is, return a
 * `map` containing `"foo"->"bar", "baz"->"rab"`.
 */
std::map<std::string, std::string> ExtractMDFromHeader(std::string header) {
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
 * Given a `method`, extract its `google.api.http` option and parse it.
 *
 * The expected format of the option is
 * `something{foo=bar}something_else{baz=rab}`. For such a content, a `map`
 * containing `"foo"->"bar", "baz"->"rab"` is returned.
 */
std::map<std::string, std::string> FromHttpRule(
    google::api::HttpRule const& http,
    absl::optional<std::string> const& resource_name) {
  std::map<std::string, std::string> headers;
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
  while (srv_cq_->Next(&placeholder, &ok))
    ;
  while (cli_cq_.Next(&placeholder, &ok))
    ;
}

std::multimap<std::string, std::string> ValidateMetadataFixture::GetMetadata(
    grpc::ClientContext& context) {
  // Set the deadline to far in the future. If the deadline is in the past,
  // gRPC doesn't send the initial metadata at all (which makes sense, given
  // that the context is already expired). The `context` is destroyed by this
  // function anyway, so we're not making things worse by changing the
  // deadline.
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::hours(24));

  // Send some garbage with the supplied context.
  grpc::GenericStub generic_stub(
      server_->InProcessChannel(grpc::ChannelArguments()));

  auto cli_stream =
      generic_stub.PrepareCall(&context, "made_up_method", &cli_cq_);
  cli_stream->StartCall(nullptr);
  bool ok;
  void* placeholder;
  cli_cq_.Next(&placeholder, &ok);  // actually start the client call

  // Receive the garbage with the supplied context.
  grpc::GenericServerContext server_context;
  grpc::GenericServerAsyncReaderWriter reader_writer(&server_context);
  generic_service_.RequestCall(&server_context, &reader_writer, srv_cq_.get(),
                               srv_cq_.get(), nullptr);
  srv_cq_->Next(&placeholder, &ok);  // actually receive the data

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

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

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

#include "google/cloud/internal/rest_stub_helpers.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Status RestResponseToProto(google::protobuf::Message& destination,
                           RestResponse&& rest_response) {
  if (rest_response.StatusCode() != HttpStatusCode::kOk) {
    return AsStatus(std::move(rest_response));
  }
  auto json_response =
      rest_internal::ReadAll(std::move(rest_response).ExtractPayload());
  if (!json_response.ok()) return std::move(json_response).status();
  google::protobuf::util::JsonParseOptions parse_options;
  parse_options.ignore_unknown_fields = true;
  auto json_to_proto_status = google::protobuf::util::JsonStringToMessage(
      *json_response, &destination, parse_options);
  if (!json_to_proto_status.ok()) {
    return Status(
        static_cast<StatusCode>(json_to_proto_status.code()),
        std::string(json_to_proto_status.message()),
        GCP_ERROR_INFO()
            .WithReason("Failure creating proto Message from Json")
            .WithMetadata("message_type", destination.GetTypeName())
            .WithMetadata("json_string", *json_response)
            .Build(static_cast<StatusCode>(json_to_proto_status.code())));
  }
  return {};
}

StatusOr<std::string> ProtoRequestToJsonPayload(
    google::protobuf::Message const& request, bool preserve_proto_field_names) {
  std::string json_payload;
  google::protobuf::util::JsonPrintOptions print_options;
  print_options.preserve_proto_field_names = preserve_proto_field_names;
  auto proto_to_json_status = google::protobuf::util::MessageToJsonString(
      request, &json_payload, print_options);

  if (!proto_to_json_status.ok()) {
    return internal::InternalError(
        std::string(proto_to_json_status.message()),
        GCP_ERROR_INFO()
            .WithReason("Failure converting proto request to HTTP")
            .WithMetadata("message_type", request.GetTypeName()));
  }
  return json_payload;
}

rest_internal::RestRequest CreateRestRequest(
    std::string path,
    std::vector<std::pair<std::string, std::string>> query_params) {
  rest_internal::RestRequest rest_request;
  rest_request.SetPath(std::move(path));
  for (auto& p : query_params) {
    rest_request.AddQueryParameter(std::move(p));
  }
  return rest_request;
}

std::vector<std::pair<std::string, std::string>> TrimEmptyQueryParameters(
    std::vector<std::pair<std::string, std::string>> query_params) {
  std::vector<std::pair<std::string, std::string>> trimmed_params;
  for (auto& qp : query_params) {
    if (!qp.first.empty() && !qp.second.empty()) {
      trimmed_params.push_back(std::move(qp));
    }
  }
  return trimmed_params;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

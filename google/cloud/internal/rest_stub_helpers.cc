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
  if (!json_response.ok()) return json_response.status();
  auto json_to_proto_status =
      google::protobuf::util::JsonStringToMessage(*json_response, &destination);
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

Status ProtoRequestToJsonPayload(google::protobuf::Message const& request,
                                 std::string& json_payload) {
  protobuf::util::Status proto_to_json_status =
      protobuf::util::MessageToJsonString(request, &json_payload);
  if (!proto_to_json_status.ok()) {
    return Status{
        StatusCode::kInternal, std::string{proto_to_json_status.message()}, {}};
  }
  return {};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

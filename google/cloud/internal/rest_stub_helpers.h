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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_STUB_HELPERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_STUB_HELPERS_H

#include "google/cloud/internal/http_payload.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/internal/rest_request.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "google/protobuf/util/json_util.h"
#include <string>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <typename Response>
StatusOr<Response> RestResponseToProto(RestResponse&& rest_response) {
  if (rest_response.StatusCode() != HttpStatusCode::kOk) {
    return AsStatus(std::move(rest_response));
  }
  auto json_response =
      rest_internal::ReadAll(std::move(rest_response).ExtractPayload());
  if (!json_response.ok()) return json_response.status();
  Response proto_response;
  auto json_to_proto_status = google::protobuf::util::JsonStringToMessage(
      *json_response, &proto_response);
  return proto_response;
}

template <typename Request>
Status Delete(rest_internal::RestClient& client,
              rest_internal::RestContext& rest_context, Request const&,
              std::string path) {
  rest_internal::RestRequest rest_request(rest_context);
  rest_request.SetPath(std::move(path));
  auto response = client.Delete(rest_request);
  if (!response.ok()) return response.status();
  return AsStatus(std::move(**response));
}

template <typename Response, typename Request>
StatusOr<Response> Delete(rest_internal::RestClient& client,
                          rest_internal::RestContext& rest_context,
                          Request const&, std::string path) {
  rest_internal::RestRequest rest_request(rest_context);
  rest_request.SetPath(std::move(path));
  auto response = client.Delete(rest_request);
  if (!response.ok()) return response.status();
  return RestResponseToProto<Response>(std::move(**response));
}

template <typename Response, typename Request>
StatusOr<Response> Get(
    rest_internal::RestClient& client, rest_internal::RestContext& rest_context,
    Request const&, std::string path,
    std::vector<std::pair<std::string, std::string>> query_params = {}) {
  rest_internal::RestRequest rest_request(rest_context);
  for (auto& p : query_params) {
    rest_request.AddQueryParameter(std::move(p));
  }
  rest_request.SetPath(std::move(path));
  auto response = client.Get(rest_request);
  if (!response.ok()) return response.status();
  return RestResponseToProto<Response>(std::move(**response));
}

template <typename Response, typename Request>
StatusOr<Response> Patch(rest_internal::RestClient& client,
                         rest_internal::RestContext& rest_context,
                         Request const& request, std::string path) {
  std::string json_payload;
  protobuf::util::Status proto_to_json_status =
      protobuf::util::MessageToJsonString(request, &json_payload);
  if (!proto_to_json_status.ok())
    return Status{
        StatusCode::kInternal, std::string{proto_to_json_status.message()}, {}};
  rest_internal::RestRequest rest_request(rest_context);
  rest_request.SetPath(std::move(path));
  rest_request.AddHeader("content-type", "application/json");
  auto response =
      client.Patch(rest_request, {absl::MakeConstSpan(json_payload)});
  if (!response.ok()) return response.status();
  return RestResponseToProto<Response>(std::move(**response));
}

template <typename Response, typename Request>
StatusOr<Response> Post(
    rest_internal::RestClient& client, rest_internal::RestContext& rest_context,
    Request const& request, std::string path,
    std::vector<std::pair<std::string, std::string>> query_params = {}) {
  std::string json_payload;
  protobuf::util::Status proto_to_json_status =
      protobuf::util::MessageToJsonString(request, &json_payload);
  if (!proto_to_json_status.ok())
    return Status{
        StatusCode::kInternal, std::string{proto_to_json_status.message()}, {}};
  rest_internal::RestRequest rest_request(rest_context);
  rest_request.SetPath(std::move(path));
  for (auto& p : query_params) {
    rest_request.AddQueryParameter(std::move(p));
  }
  rest_request.AddHeader("content-type", "application/json");
  auto response =
      client.Post(rest_request, {absl::MakeConstSpan(json_payload)});
  if (!response.ok()) return response.status();
  return RestResponseToProto<Response>(std::move(**response));
}

template <typename Request>
Status Post(
    rest_internal::RestClient& client, rest_internal::RestContext& rest_context,
    Request const& request, std::string path,
    std::vector<std::pair<std::string, std::string>> query_params = {}) {
  std::string json_payload;
  protobuf::util::Status proto_to_json_status =
      protobuf::util::MessageToJsonString(request, &json_payload);
  if (!proto_to_json_status.ok())
    return Status{
        StatusCode::kInternal, std::string{proto_to_json_status.message()}, {}};
  rest_internal::RestRequest rest_request(rest_context);
  rest_request.SetPath(std::move(path));
  for (auto& p : query_params) {
    rest_request.AddQueryParameter(std::move(p));
  }
  rest_request.AddHeader("content-type", "application/json");
  auto response =
      client.Post(rest_request, {absl::MakeConstSpan(json_payload)});
  return AsStatus(std::move(**response));
}

template <typename Response, typename Request>
StatusOr<Response> Put(rest_internal::RestClient& client,
                       rest_internal::RestContext& rest_context,
                       Request const& request, std::string path) {
  std::string json_payload;
  protobuf::util::Status proto_to_json_status =
      protobuf::util::MessageToJsonString(request, &json_payload);
  if (!proto_to_json_status.ok())
    return Status{
        StatusCode::kInternal, std::string{proto_to_json_status.message()}, {}};
  rest_internal::RestRequest rest_request(rest_context);
  rest_request.SetPath(std::move(path));
  rest_request.AddHeader("content-type", "application/json");
  auto response = client.Put(rest_request, {absl::MakeConstSpan(json_payload)});
  if (!response.ok()) return response.status();
  return RestResponseToProto<Response>(std::move(**response));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_STUB_HELPERS_H

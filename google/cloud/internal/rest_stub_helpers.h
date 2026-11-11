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
#include "absl/strings/str_cat.h"
#include <google/protobuf/util/json_util.h>
#include <initializer_list>
#include <string>
#include <string_view>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct EmptyResponseType {};

std::vector<std::pair<std::string, std::string>> TrimEmptyQueryParameters(
    std::vector<std::pair<std::string, std::string>> query_params);

Status RestResponseToProto(google::protobuf::Message& destination,
                           RestResponse&& rest_response);

StatusOr<std::string> ProtoRequestToJsonPayload(
    google::protobuf::Message const& request, bool preserve_proto_field_names);

rest_internal::RestRequest CreateRestRequest(
    std::string path,
    std::vector<std::pair<std::string, std::string>> query_params);

inline std::string const& DetermineApiVersion(
    std::string const& default_version, Options const& opts) {
  return opts.has<rest_internal::TargetApiVersionOption>()
             ? opts.get<rest_internal::TargetApiVersionOption>()
             : default_version;
}

/// A non-owning view of a piece of a REST URI path used with `EncodePath`.
///
/// Plain arguments convertible to `absl::AlphaNum` (strings,
/// `std::string_view`, integers) construct a single-segment `PathPiece` inside
/// `EncodePath(...)`, which rejects `.`, `..`, and `/` and percent-encodes
/// characters outside `[-_.~0-9a-zA-Z]`.
///
/// Use `PathPiece::MultiSegment(...)` for variables whose URI template spans
/// multiple `/` segments (e.g. `{parent=projects/*/locations/*}` or
/// `{name=**}`), and `PathPiece::Verb(...)` for trailing custom verbs (e.g.
/// `:cancel`).
///
/// @note Like `std::string_view`, `PathPiece` does not own the underlying
/// character buffer and must only be used as a temporary argument to
/// `EncodePath(...)`.
class PathPiece {
 public:
  enum class EncodingKind {
    kSingleSegment,
    kMultiSegment,
    kVerb,
  };

  static PathPiece MultiSegment(std::string_view value) {
    return PathPiece(value, EncodingKind::kMultiSegment);
  }

  static PathPiece Verb(std::string_view value) {
    return PathPiece(value, EncodingKind::kVerb);
  }

  PathPiece(PathPiece const&) = default;
  PathPiece(PathPiece&&) = default;
  PathPiece& operator=(PathPiece const&) = delete;
  PathPiece& operator=(PathPiece&&) = delete;

  std::string_view value() const { return value_; }
  EncodingKind kind() const { return kind_; }

 private:
  template <typename... Pieces>
  friend StatusOr<std::string> EncodePath(Pieces const&... pieces);

  explicit PathPiece(absl::AlphaNum const& value)
      : value_(value.Piece().data(), value.Piece().size()),
        kind_(EncodingKind::kSingleSegment) {}

  PathPiece(std::string_view value, EncodingKind kind)
      : value_(value), kind_(kind) {}

  std::string_view value_;
  EncodingKind kind_;
};

StatusOr<std::string> EncodePathImpl(std::initializer_list<PathPiece> pieces);

/// Validates and percent-encodes a sequence of path pieces into a REST URI
/// path, prepending and joining segments with `'/'`.
template <typename... Pieces>
StatusOr<std::string> EncodePath(Pieces const&... pieces) {
  return EncodePathImpl({PathPiece(pieces)...});
}

template <typename Response>
StatusOr<Response> RestResponseToProto(RestResponse&& rest_response) {
  Response destination;
  auto status = RestResponseToProto(destination, std::move(rest_response));
  if (!status.ok()) return status;
  return destination;
}

template <
    typename Response, typename Request,
    std::enable_if_t<std::is_same<Response, EmptyResponseType>::value, int> = 0>
Status Delete(
    rest_internal::RestClient& client, rest_internal::RestContext& rest_context,
    Request const&, bool, std::string path,
    std::vector<std::pair<std::string, std::string>> query_params = {}) {
  auto rest_request =
      CreateRestRequest(std::move(path), std::move(query_params));
  auto response = client.Delete(rest_context, rest_request);
  if (!response.ok()) return response.status();
  return AsStatus(std::move(**response));
}

template <typename Response, typename Request,
          std::enable_if_t<!std::is_same<Response, EmptyResponseType>::value,
                           int> = 0>
StatusOr<Response> Delete(
    rest_internal::RestClient& client, rest_internal::RestContext& rest_context,
    Request const&, bool, std::string path,
    std::vector<std::pair<std::string, std::string>> query_params = {}) {
  auto rest_request =
      CreateRestRequest(std::move(path), std::move(query_params));
  auto response = client.Delete(rest_context, rest_request);
  if (!response.ok()) return response.status();
  return RestResponseToProto<Response>(std::move(**response));
}

template <typename Response, typename Request,
          std::enable_if_t<!std::is_same<Response, EmptyResponseType>::value,
                           int> = 0>
StatusOr<Response> Get(
    rest_internal::RestClient& client, rest_internal::RestContext& rest_context,
    Request const&, bool, std::string path,
    std::vector<std::pair<std::string, std::string>> query_params = {}) {
  auto rest_request =
      CreateRestRequest(std::move(path), std::move(query_params));
  auto response = client.Get(rest_context, rest_request);
  if (!response.ok()) return response.status();
  return RestResponseToProto<Response>(std::move(**response));
}

template <
    typename Response, typename Request,
    std::enable_if_t<std::is_same<Response, EmptyResponseType>::value, int> = 0>
Status Get(rest_internal::RestClient& client,
           rest_internal::RestContext& rest_context, Request const&, bool,
           std::string path,
           std::vector<std::pair<std::string, std::string>> query_params = {}) {
  auto rest_request =
      CreateRestRequest(std::move(path), std::move(query_params));
  auto response = client.Get(rest_context, rest_request);
  if (!response.ok()) return response.status();
  return Status();
}

template <typename Response, typename Request>
StatusOr<Response> Patch(
    rest_internal::RestClient& client, rest_internal::RestContext& rest_context,
    Request const& request, bool preserve_proto_field_names, std::string path,
    std::vector<std::pair<std::string, std::string>> query_params = {}) {
  auto json_payload =
      ProtoRequestToJsonPayload(request, preserve_proto_field_names);
  if (!json_payload.ok()) return std::move(json_payload).status();
  auto rest_request =
      CreateRestRequest(std::move(path), std::move(query_params));
  rest_request.AddHeader("content-type", "application/json");
  auto response = client.Patch(rest_context, rest_request,
                               {absl::MakeConstSpan(*json_payload)});
  if (!response.ok()) return response.status();
  return RestResponseToProto<Response>(std::move(**response));
}

template <typename Response, typename Request,
          std::enable_if_t<!std::is_same<Response, EmptyResponseType>::value,
                           int> = 0>
StatusOr<Response> Post(
    rest_internal::RestClient& client, rest_internal::RestContext& rest_context,
    Request const& request, bool preserve_proto_field_names, std::string path,
    std::vector<std::pair<std::string, std::string>> query_params = {}) {
  auto json_payload =
      ProtoRequestToJsonPayload(request, preserve_proto_field_names);
  if (!json_payload.ok()) return std::move(json_payload).status();
  auto rest_request =
      CreateRestRequest(std::move(path), std::move(query_params));
  rest_request.AddHeader("content-type", "application/json");
  auto response = client.Post(rest_context, rest_request,
                              {absl::MakeConstSpan(*json_payload)});
  if (!response.ok()) return response.status();
  return RestResponseToProto<Response>(std::move(**response));
}

template <
    typename Response, typename Request,
    std::enable_if_t<std::is_same<Response, EmptyResponseType>::value, int> = 0>
Status Post(
    rest_internal::RestClient& client, rest_internal::RestContext& rest_context,
    Request const& request, bool preserve_proto_field_names, std::string path,
    std::vector<std::pair<std::string, std::string>> query_params = {}) {
  auto json_payload =
      ProtoRequestToJsonPayload(request, preserve_proto_field_names);
  if (!json_payload.ok()) return std::move(json_payload).status();
  auto rest_request =
      CreateRestRequest(std::move(path), std::move(query_params));
  rest_request.AddHeader("content-type", "application/json");
  auto response = client.Post(rest_context, rest_request,
                              {absl::MakeConstSpan(*json_payload)});
  if (!response.ok()) return response.status();
  return AsStatus(std::move(**response));
}

template <typename Response, typename Request>
StatusOr<Response> Put(
    rest_internal::RestClient& client, rest_internal::RestContext& rest_context,
    Request const& request, bool preserve_proto_field_names, std::string path,
    std::vector<std::pair<std::string, std::string>> query_params = {}) {
  auto json_payload =
      ProtoRequestToJsonPayload(request, preserve_proto_field_names);
  if (!json_payload.ok()) return std::move(json_payload).status();
  auto rest_request =
      CreateRestRequest(std::move(path), std::move(query_params));
  rest_request.AddHeader("content-type", "application/json");
  auto response = client.Put(rest_context, rest_request,
                             {absl::MakeConstSpan(*json_payload)});
  if (!response.ok()) return response.status();
  return RestResponseToProto<Response>(std::move(**response));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_STUB_HELPERS_H

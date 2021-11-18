// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_DETAIL_PROTO_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_DETAIL_PROTO_H

#include "google/cloud/internal/status_payload_keys.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <google/protobuf/any.pb.h>
#include <google/rpc/status.pb.h>
#include <type_traits>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace internal {
template <typename T>
absl::optional<T> GetStatusDetailProto(google::rpc::Status const&);
}  // namespace internal

/**
 * Gets the "error details" protobuf of type `T` from the given status.
 *
 * Error details objects are protocol buffers that may be attached to non-OK
 * Status objects from gRPC to provide more details about the error. The
 * message types are defined in this proto:
 * https://github.com/googleapis/googleapis/blob/master/google/rpc/error_details.proto
 *
 * The following shows how to get a `google::rpc::ErrorInfo` message:
 *
 * @code
 * #include <absl/types/optional.h>
 * #include <google/rpc/error_details.pb.h>
 * ...
 *   google::cloud::Status status = ...
 *   absl::optional<google::rpc::ErrorInfo> ei =
 *       google::cloud::GetStatusDetailProto<google::rpc::ErrorInfo>(status);
 * @endcode
 *
 * @see https://google.aip.dev/193
 */
template <typename T>
absl::optional<T> GetStatusDetailProto(Status const& s) {
  static_assert(std::is_base_of<google::protobuf::Message, T>::value,
                "The template parameter T must be a protobuf message");
  auto payload = internal::GetPayload(s, internal::kStatusPayloadGrpcProto);
  if (!payload) return absl::nullopt;
  google::rpc::Status proto;
  if (!proto.ParseFromString(*payload)) return absl::nullopt;
  return internal::GetStatusDetailProto<T>(proto);
}

namespace internal {
// A private helper, not for public use.
template <typename T>
absl::optional<T> GetStatusDetailProto(google::rpc::Status const& proto) {
  for (google::protobuf::Any const& any : proto.details()) {
    if (any.Is<T>()) {
      T details;
      if (any.UnpackTo(&details)) return details;
    }
  }
  return absl::nullopt;
}
}  // namespace internal

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_DETAIL_PROTO_H

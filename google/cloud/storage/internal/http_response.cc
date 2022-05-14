// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include <nlohmann/json.hpp>
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

namespace {

// Maps HTTP status codes to enumerators in `StatusCode`. The code here is
// organized by increasing range (or value) of the errors, just to make it
// readable. There are probably shorter (and/or more efficient) ways to write
// this, but we went for readability.
StatusCode MapHttpCodeToStatus(long code) {  // NOLINT(google-runtime-int)
  // We treat the 100s (e.g. 100 Continue) as OK results. They normally are
  // ignored by libcurl, so we do not really expect to see them.
  if (HttpStatusCode::kMinContinue <= code &&
      code < HttpStatusCode::kMinSuccess) {
    return StatusCode::kOk;
  }
  // We treat the 200s as Okay results.
  if (HttpStatusCode::kMinSuccess <= code &&
      code < HttpStatusCode::kMinRedirects) {
    return StatusCode::kOk;
  }
  if (code == HttpStatusCode::kResumeIncomplete) {
    // 308 - Resume Incomplete: this one is terrible. When performing a PUT
    // for a resumable upload this means "The client and server are out of sync
    // in this resumable upload, please reset". Unfortunately, during a
    // "reset" this means "The reset worked, here is the next committed byte,
    // keep in mind that the server is still doing work".  The second is more
    // like a kOk, the first is more like a kFailedPrecondition.
    // This level of complexity / detail is something that the caller should
    // handle, i.e., the mapping depends on the operation.
    return StatusCode::kFailedPrecondition;
  }
  if (code == HttpStatusCode::kNotModified) {
    // 304 - Not Modified: evidently GCS returns 304 for some failed
    // pre-conditions. It is somewhat strange that it also returns this error
    // code for downloads, which is always read-only and was not going to modify
    // anything. In any case, it seems too confusing to return anything other
    // than kFailedPrecondition here.
    return StatusCode::kFailedPrecondition;
  }
  if (HttpStatusCode::kMinRedirects <= code &&
      code < HttpStatusCode::kMinRequestErrors) {
    // The 300s should be handled by libcurl, we should not get them, according
    // to the Google Cloud Storage documentation these are:
    // 302 - Found
    // 303 - See Other
    // 307 - Temporary Redirect
    return StatusCode::kUnknown;
  }
  if (code == HttpStatusCode::kBadRequest) {
    return StatusCode::kInvalidArgument;
  }
  if (code == HttpStatusCode::kUnauthorized) {
    return StatusCode::kUnauthenticated;
  }
  if (code == HttpStatusCode::kForbidden) {
    return StatusCode::kPermissionDenied;
  }
  if (code == HttpStatusCode::kNotFound) {
    return StatusCode::kNotFound;
  }
  if (code == HttpStatusCode::kMethodNotAllowed) {
    return StatusCode::kPermissionDenied;
  }
  if (code == HttpStatusCode::kRequestTimeout) {
    // GCS uses a 408 to signal that an upload has suffered a broken
    // connection, and that the client should retry.
    return StatusCode::kUnavailable;
  }
  if (code == HttpStatusCode::kConflict) {
    return StatusCode::kAborted;
  }
  if (code == HttpStatusCode::kGone) {
    return StatusCode::kNotFound;
  }
  if (code == HttpStatusCode::kLengthRequired) {
    return StatusCode::kInvalidArgument;
  }
  if (code == HttpStatusCode::kPreconditionFailed) {
    return StatusCode::kFailedPrecondition;
  }
  if (code == HttpStatusCode::kPayloadTooLarge) {
    return StatusCode::kOutOfRange;
  }
  if (code == HttpStatusCode::kRequestRangeNotSatisfiable) {
    return StatusCode::kOutOfRange;
  }
  if (code == HttpStatusCode::kTooManyRequests) {
    return StatusCode::kUnavailable;
  }
  if (HttpStatusCode::kMinRequestErrors <= code &&
      code < HttpStatusCode::kMinInternalErrors) {
    // 4XX - A request error.
    return StatusCode::kInvalidArgument;
  }
  if (code == HttpStatusCode::kInternalServerError) {
    return StatusCode::kUnavailable;
  }
  if (code == HttpStatusCode::kBadGateway) {
    return StatusCode::kUnavailable;
  }
  if (code == HttpStatusCode::kServiceUnavailable) {
    return StatusCode::kUnavailable;
  }
  if (HttpStatusCode::kMinInternalErrors <= code &&
      code < HttpStatusCode::kMinInvalidCode) {
    // 5XX - server errors are mapped to kInternal.
    return StatusCode::kInternal;
  }
  return StatusCode::kUnknown;
}

// Makes an `ErrorInfo` from an `"error"` JSON object that looks like
//   [
//     {
//       "@type": "type.googleapis.com/google.rpc.ErrorInfo",
//       "reason": "..."
//       "domain": "..."
//       "metadata": {
//         "key1": "value1"
//         ...
//       }
//     }
//   ]
// See also https://cloud.google.com/apis/design/errors#http_mapping
ErrorInfo MakeErrorInfo(nlohmann::json const& details) {
  static auto constexpr kErrorInfoType =
      "type.googleapis.com/google.rpc.ErrorInfo";
  if (!details.is_array()) return ErrorInfo{};
  for (auto const& e : details.items()) {
    auto const& v = e.value();
    if (!v.is_object()) continue;
    if (!v.contains("@type") || !v["@type"].is_string()) continue;
    if (v.value("@type", "") != kErrorInfoType) continue;
    if (!v.contains("reason") || !v["reason"].is_string()) continue;
    if (!v.contains("domain") || !v["domain"].is_string()) continue;
    if (!v.contains("metadata") || !v["metadata"].is_object()) continue;
    auto const& metadata_json = v["metadata"];
    auto metadata = std::unordered_map<std::string, std::string>{};
    for (auto const& m : metadata_json.items()) {
      if (!m.value().is_string()) continue;
      metadata[m.key()] = m.value();
    }
    return ErrorInfo{v.value("reason", ""), v.value("domain", ""),
                     std::move(metadata)};
  }
  return ErrorInfo{};
}

}  // namespace

Status AsStatus(HttpResponse const& http_response) {
  auto const code = MapHttpCodeToStatus(http_response.status_code);
  if (code == StatusCode::kOk) return Status{};

  auto default_error = [&] { return Status(code, http_response.payload); };

  // We try to parse the payload as JSON, which may allow us to provide a more
  // structured and useful error Status. If the payload fails to parse as JSON,
  // we simply attach the full error payload as the Status's message string.
  auto json = nlohmann::json::parse(http_response.payload, nullptr, false);
  if (!json.is_object()) return default_error();

  // We expect JSON that looks like the following:
  //   {
  //     "error": {
  //       "message": "..."
  //       ...
  //       "details": [
  //         ...
  //       ]
  //     }
  //   }
  // See  https://cloud.google.com/apis/design/errors#http_mapping
  if (!json.contains("error")) return default_error();
  auto const& e = json["error"];
  if (!e.is_object()) return default_error();
  if (!e.contains("message") || !e.contains("details")) return default_error();
  if (!e["message"].is_string()) return default_error();

  return Status(code, e.value("message", ""), MakeErrorInfo(e["details"]));
}

std::ostream& operator<<(std::ostream& os, HttpResponse const& rhs) {
  os << "status_code=" << rhs.status_code << ", headers={";
  os << absl::StrJoin(rhs.headers, ", ", absl::PairFormatter(": "));
  return os << "}, payload=<" << rhs.payload << ">";
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

// Copyright 2026 Google LLC
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

#include "google/cloud/storage/internal/retry_logging.h"
#include "google/cloud/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// A stable, greppable prefix. Applications aggregating logs across a fleet
// filter on this to isolate service-side throttling from other warnings.
auto constexpr kRetryLogPrefix = "[gcs-retry]";

// Renders @p status as a single line.
//
// The message is whatever the service sent back. GCS error bodies routinely
// end with a newline, and an embedded newline would split one record across
// several lines: `grep '[gcs-retry]'` would then show a truncated record, and
// a service returning a crafted message could forge log lines outright.
std::string OneLine(Status const& status) {
  std::ostringstream os;
  os << status;
  std::string text = std::move(os).str();
  std::replace_if(
      text.begin(), text.end(),
      [](char c) { return c == '\n' || c == '\r' || c == '\t'; }, ' ');
  while (!text.empty() && text.back() == ' ') text.pop_back();
  return text;
}

// Long enough to tell two concurrent uploads apart, short enough to be useless
// to anyone who wants to take one over.
std::size_t constexpr kRetainedTokenChars = 8;
auto constexpr kRedactedMarker = std::string_view{"...[redacted]"};

// Keeps the first `kRetainedTokenChars` of `[start, end)` and replaces the rest
// of that range with `kRedactedMarker`, leaving the surrounding text intact.
std::string RedactRange(std::string_view text, std::size_t start,
                        std::size_t end) {
  std::size_t const kept = (std::min)(kRetainedTokenChars, end - start);
  return absl::StrCat(text.substr(0, start + kept), kRedactedMarker,
                      text.substr(end));
}

}  // namespace

std::string RetryLogResource(std::string_view bucket_name,
                             std::string_view object_name) {
  if (bucket_name.empty()) return std::string(object_name);
  if (object_name.empty()) return std::string(bucket_name);
  return absl::StrCat(bucket_name, "/", object_name);
}

std::string RetryLogUploadResource(std::string_view upload_session_url) {
  if (upload_session_url.empty()) return {};

  // REST: the token is the value of the `upload_id` query parameter. The rest
  // of the URL is the bucket and the upload type, neither of which is secret.
  auto constexpr kUploadId = std::string_view{"upload_id="};
  std::size_t const pos = upload_session_url.find(kUploadId);
  if (pos != std::string_view::npos) {
    std::size_t const start = pos + kUploadId.size();
    std::size_t end = upload_session_url.find('&', start);
    if (end == std::string_view::npos) end = upload_session_url.size();
    return RedactRange(upload_session_url, start, end);
  }

  // gRPC, resource-name form: `projects/<project>/buckets/<bucket>/<token>`.
  // Keep the bucket, redact the rest. The token may contain '/' itself, so
  // everything past the bucket segment goes. This shape is indistinguishable
  // from an object resource name; it is only safe to treat the tail as a token
  // because this function is documented to take an upload session identifier.
  auto constexpr kProjects = std::string_view{"projects/"};
  auto constexpr kBuckets = std::string_view{"/buckets/"};
  if (absl::StartsWith(upload_session_url, kProjects)) {
    std::size_t const buckets = upload_session_url.find(kBuckets);
    if (buckets != std::string_view::npos) {
      std::size_t const bucket_start = buckets + kBuckets.size();
      std::size_t const token_start =
          upload_session_url.find('/', bucket_start);
      // A bucket and no trailing segment: there is no token to hide.
      if (token_start == std::string_view::npos) {
        return std::string(upload_session_url);
      }
      return RedactRange(upload_session_url, token_start + 1,
                         upload_session_url.size());
    }
  }

  // Anything else is redacted whole.
  //
  // This is deliberately fail-closed. The argument is documented to be an
  // upload session identifier, and gRPC hands back an opaque token with no
  // structure at all -- the emulator, for instance, returns a bare 64
  // character hex string. Returning an unrecognised value verbatim would
  // publish the entire bearer capability the moment the service changes the
  // format, which is exactly the case least likely to be noticed. Keeping a
  // short prefix is still enough to correlate the records of one upload.
  return RedactRange(upload_session_url, 0, upload_session_url.size());
}

void LogTransientRetry(char const* where, std::string_view resource,
                       Status const& status, int attempt) {
  // Note this is intentionally a single line: these are emitted once per failed
  // attempt, and during a large-scale throttling event many streams fail at
  // once.
  if (resource.empty()) {
    GCP_LOG(WARNING) << kRetryLogPrefix << " " << where
                     << ": transient error on attempt " << attempt
                     << ", status=" << OneLine(status);
    return;
  }
  GCP_LOG(WARNING) << kRetryLogPrefix << " " << where
                   << ": transient error on attempt " << attempt
                   << ", resource=" << resource
                   << ", status=" << OneLine(status);
}

LoggingRetryPolicy::LoggingRetryPolicy(google::cloud::RetryPolicy& impl,
                                       char const* where, std::string resource)
    : impl_(impl), where_(where), resource_(std::move(resource)) {}

bool LoggingRetryPolicy::OnFailure(Status const& status) {
  // Ask the policy first. Logging only when it says "retry" keeps the record
  // honest: every line published corresponds to an attempt that really is
  // followed by another one.
  bool const retrying = impl_.OnFailure(status);
  if (!retrying) return false;
  LogTransientRetry(where_, resource_, status, ++attempt_);
  return true;
}

bool LoggingRetryPolicy::IsExhausted() const { return impl_.IsExhausted(); }

bool LoggingRetryPolicy::IsPermanentFailure(Status const& status) const {
  return impl_.IsPermanentFailure(status);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

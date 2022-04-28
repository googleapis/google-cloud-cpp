// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/raw_client.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

StatusOr<std::unique_ptr<ResumableUploadSession>>
RawClient::RestoreResumableSession(std::string const& /*session_id*/) {
  return Status(StatusCode::kUnimplemented, "removed, see #7282 for details");
}

StatusOr<CreateOrResumeResponse> CreateOrResume(
    RawClient& client, ResumableUploadRequest const& request) {
  auto session_id = request.GetOption<UseResumableUploadSession>().value_or("");
  if (session_id.empty()) {
    auto create = client.CreateResumableUpload(request);
    if (!create) return std::move(create).status();
    return CreateOrResumeResponse{std::move(create->upload_id), 0,
                                  absl::nullopt};
  }

  auto query = internal::QueryResumableUploadRequest(session_id);
  auto response = client.QueryResumableUpload(query);
  if (!response) return std::move(response).status();
  return CreateOrResumeResponse{std::move(session_id),
                                response->committed_size.value_or(0),
                                std::move(response->payload)};
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/logging_resumable_upload_session.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

std::pair<Status, ResumableUploadResponse>
LoggingResumableUploadSession::UploadChunk(std::string const& buffer,
                                           std::uint64_t upload_size) {
  GCP_LOG(INFO) << __func__ << " << upload_size=" << upload_size
                << ", buffer.size=" << buffer.size();
  auto response = session_->UploadChunk(buffer, upload_size);
  GCP_LOG(INFO) << __func__ << " >> status={" << response.first
                << "}, payload={" << response.second << "}";
  return response;
}

std::pair<Status, ResumableUploadResponse>
LoggingResumableUploadSession::ResetSession() {
  GCP_LOG(INFO) << __func__ << " << ()";
  auto response = session_->ResetSession();
  GCP_LOG(INFO) << __func__ << " >> status={" << response.first
                << "}, payload={" << response.second << "}";
  return response;
}

std::uint64_t LoggingResumableUploadSession::next_expected_byte() const {
  GCP_LOG(INFO) << __func__ << " << ()";
  auto response = session_->next_expected_byte();
  GCP_LOG(INFO) << __func__ << " >> " << response;
  return response;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

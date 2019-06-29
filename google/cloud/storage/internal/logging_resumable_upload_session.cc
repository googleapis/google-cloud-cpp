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
#include <ios>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

StatusOr<ResumableUploadResponse> LoggingResumableUploadSession::UploadChunk(
    std::string const& buffer) {
  GCP_LOG(INFO) << __func__ << "(), buffer.size=" << buffer.size();
  auto response = session_->UploadChunk(buffer);
  if (response.ok()) {
    GCP_LOG(INFO) << __func__ << " >> payload={" << response.value() << "}";
  } else {
    GCP_LOG(INFO) << __func__ << " >> status={" << response.status() << "}";
  }
  return response;
}

StatusOr<ResumableUploadResponse>
LoggingResumableUploadSession::UploadFinalChunk(std::string const& buffer,
                                                std::uint64_t upload_size) {
  GCP_LOG(INFO) << __func__ << "() << upload_size=" << upload_size
                << ", buffer.size=" << buffer.size();
  auto response = session_->UploadFinalChunk(buffer, upload_size);
  if (response.ok()) {
    GCP_LOG(INFO) << __func__ << " >> payload={" << response.value() << "}";
  } else {
    GCP_LOG(INFO) << __func__ << " >> status={" << response.status() << "}";
  }
  return response;
}

StatusOr<ResumableUploadResponse>
LoggingResumableUploadSession::ResetSession() {
  GCP_LOG(INFO) << __func__ << " << ()";
  auto response = session_->ResetSession();
  if (response.ok()) {
    GCP_LOG(INFO) << __func__ << " >> payload={" << response.value() << "}";
  } else {
    GCP_LOG(INFO) << __func__ << " >> status={" << response.status() << "}";
  }
  return response;
}

std::uint64_t LoggingResumableUploadSession::next_expected_byte() const {
  GCP_LOG(INFO) << __func__ << " << ()";
  auto response = session_->next_expected_byte();
  GCP_LOG(INFO) << __func__ << " >> " << response;
  return response;
}

std::string const& LoggingResumableUploadSession::session_id() const {
  GCP_LOG(INFO) << __func__ << " << ()";
  auto const& response = session_->session_id();
  GCP_LOG(INFO) << __func__ << " >> " << response;
  return response;
}

StatusOr<ResumableUploadResponse> const&
LoggingResumableUploadSession::last_response() const {
  GCP_LOG(INFO) << __func__ << " << ()";
  auto const& response = session_->last_response();
  if (response.ok()) {
    GCP_LOG(INFO) << __func__ << " >> payload={" << response.value() << "}";
  } else {
    GCP_LOG(INFO) << __func__ << " >> status={" << response.status() << "}";
  }
  return response;
}

bool LoggingResumableUploadSession::done() const { return session_->done(); }

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

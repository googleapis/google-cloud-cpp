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

#include "google/cloud/storage/internal/logging_client.h"
#include "google/cloud/log.h"
#include "google/cloud/storage/internal/raw_client_wrapper_utils.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using namespace google::cloud::storage::internal::raw_client_wrapper_utils;
/**
 * Call a RawClient operation logging both the input and the result.
 *
 * @tparam MemberFunction the signature of the member function.
 * @param client the storage::RawClient object to make the call through.
 * @param function the pointer to the member function to call.
 * @param request an initialized request parameter for the call.
 * @param error_message include this message in any exception or error log.
 * @return the result from making the call;
 */
template <typename MemberFunction>
static typename std::enable_if<
    CheckSignature<MemberFunction>::value,
    typename CheckSignature<MemberFunction>::ReturnType>::type
MakeCall(google::cloud::storage::internal::RawClient& client,
         MemberFunction function,
         typename CheckSignature<MemberFunction>::RequestType const& request,
         char const* context) {
  GCP_LOG(INFO) << context << " << " << request;
  auto response = (client.*function)(request);
  GCP_LOG(INFO) << context << " >> status={" << response.first << "}, payload={"
                << response.second << "}";
  return response;
}
}  // namespace

LoggingClient::LoggingClient(std::shared_ptr<RawClient> client)
    : client_(std::move(client)) {}

ClientOptions const& LoggingClient::client_options() const {
  return client_->client_options();
}

std::pair<Status, BucketMetadata> LoggingClient::GetBucketMetadata(
    internal::GetBucketMetadataRequest const& request) {
  return MakeCall(*client_, &RawClient::GetBucketMetadata, request, __func__);
}

std::pair<Status, ObjectMetadata> LoggingClient::InsertObjectMedia(
    internal::InsertObjectMediaRequest const& request) {
  return MakeCall(*client_, &RawClient::InsertObjectMedia, request, __func__);
}

std::pair<Status, ReadObjectRangeResponse> LoggingClient::ReadObjectRangeMedia(
    internal::ReadObjectRangeRequest const& request) {
  return MakeCall(*client_, &RawClient::ReadObjectRangeMedia, request,
                  __func__);
}

std::pair<Status, internal::ListObjectsResponse> LoggingClient::ListObjects(
    internal::ListObjectsRequest const& request) {
  return MakeCall(*client_, &RawClient::ListObjects, request, __func__);
}

std::pair<Status, internal::EmptyResponse> LoggingClient::DeleteObject(
    google::cloud::storage::internal::DeleteObjectRequest const& request) {
  return MakeCall(*client_, &RawClient::DeleteObject, request, __func__);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

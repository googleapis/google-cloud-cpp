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
using raw_client_wrapper_utils::CheckSignature;
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
MakeCall(RawClient& client, MemberFunction function,
         typename CheckSignature<MemberFunction>::RequestType const& request,
         char const* context) {
  GCP_LOG(INFO) << context << " << " << request;
  auto response = (client.*function)(request);
  GCP_LOG(INFO) << context << " >> status={" << response.first << "}, payload={"
                << response.second << "}";
  return response;
}

/**
 * Call a RawClient operation logging only the input.
 *
 * This is useful when the result is not something you can easily log, such as
 * a pointer of some kind.
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
MakeCallNoResponseLogging(
    google::cloud::storage::internal::RawClient& client,
    MemberFunction function,
    typename CheckSignature<MemberFunction>::RequestType const& request,
    char const* context) {
  GCP_LOG(INFO) << context << " << " << request;
  auto response = (client.*function)(request);
  return response;
}
}  // namespace

LoggingClient::LoggingClient(std::shared_ptr<RawClient> client)
    : client_(std::move(client)) {}

ClientOptions const& LoggingClient::client_options() const {
  return client_->client_options();
}

std::pair<Status, ListBucketsResponse> LoggingClient::ListBuckets(
    ListBucketsRequest const& request) {
  return MakeCall(*client_, &RawClient::ListBuckets, request, __func__);
}

std::pair<Status, BucketMetadata> LoggingClient::GetBucketMetadata(
    GetBucketMetadataRequest const& request) {
  return MakeCall(*client_, &RawClient::GetBucketMetadata, request, __func__);
}

std::pair<Status, ObjectMetadata> LoggingClient::InsertObjectMedia(
    InsertObjectMediaRequest const& request) {
  return MakeCall(*client_, &RawClient::InsertObjectMedia, request, __func__);
}

std::pair<Status, ObjectMetadata> LoggingClient::GetObjectMetadata(
    GetObjectMetadataRequest const& request) {
  return MakeCall(*client_, &RawClient::GetObjectMetadata, request, __func__);
}

std::pair<Status, std::unique_ptr<ObjectReadStreambuf>>
LoggingClient::ReadObject(ReadObjectRangeRequest const& request) {
  return MakeCallNoResponseLogging(*client_, &RawClient::ReadObject, request,
                                   __func__);
}

std::pair<Status, std::unique_ptr<ObjectWriteStreambuf>>
LoggingClient::WriteObject(InsertObjectStreamingRequest const& request) {
  return MakeCallNoResponseLogging(*client_, &RawClient::WriteObject, request,
                                   __func__);
}

std::pair<Status, ListObjectsResponse> LoggingClient::ListObjects(
    ListObjectsRequest const& request) {
  return MakeCall(*client_, &RawClient::ListObjects, request, __func__);
}

std::pair<Status, EmptyResponse> LoggingClient::DeleteObject(
    DeleteObjectRequest const& request) {
  return MakeCall(*client_, &RawClient::DeleteObject, request, __func__);
}

std::pair<Status, ListObjectAclResponse> LoggingClient::ListObjectAcl(
    ListObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::ListObjectAcl, request, __func__);
}

std::pair<Status, ObjectAccessControl> LoggingClient::CreateObjectAcl(
    CreateObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::CreateObjectAcl, request, __func__);
}

std::pair<Status, EmptyResponse> LoggingClient::DeleteObjectAcl(
    ObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::DeleteObjectAcl, request, __func__);
}

std::pair<Status, ObjectAccessControl> LoggingClient::GetObjectAcl(
    ObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::GetObjectAcl, request, __func__);
}

std::pair<Status, ObjectAccessControl> LoggingClient::UpdateObjectAcl(
    UpdateObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::UpdateObjectAcl, request, __func__);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

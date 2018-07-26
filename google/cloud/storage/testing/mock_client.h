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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_CLIENT_H_

#include "google/cloud/storage/internal/raw_client.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

class MockClient : public google::cloud::storage::internal::RawClient {
 public:
  // The MOCK_* macros get confused if the return type is a compound template
  // with a comma, that is because Foo<T,R> looks like two arguments to the
  // preprocessor, but Foo<R> will look like a single argument.
  template <typename R>
  using ResponseWrapper = std::pair<google::cloud::storage::Status, R>;

  MOCK_CONST_METHOD0(client_options, ClientOptions const&());
  MOCK_METHOD1(ListBuckets, ResponseWrapper<internal::ListBucketsResponse>(
                                internal::ListBucketsRequest const&));
  MOCK_METHOD1(GetBucketMetadata,
               ResponseWrapper<storage::BucketMetadata>(
                   internal::GetBucketMetadataRequest const&));
  MOCK_METHOD1(InsertObjectMedia,
               ResponseWrapper<storage::ObjectMetadata>(
                   internal::InsertObjectMediaRequest const&));
  MOCK_METHOD1(GetObjectMetadata,
               ResponseWrapper<storage::ObjectMetadata>(
                   internal::GetObjectMetadataRequest const&));
  MOCK_METHOD1(ReadObject,
               ResponseWrapper<std::unique_ptr<internal::ObjectReadStreambuf>>(
                   internal::ReadObjectRangeRequest const&));
  MOCK_METHOD1(WriteObject,
               ResponseWrapper<std::unique_ptr<internal::ObjectWriteStreambuf>>(
                   internal::InsertObjectStreamingRequest const&));
  MOCK_METHOD1(ListObjects, ResponseWrapper<internal::ListObjectsResponse>(
                                internal::ListObjectsRequest const&));
  MOCK_METHOD1(DeleteObject, ResponseWrapper<internal::EmptyResponse>(
                                 internal::DeleteObjectRequest const&));

  MOCK_METHOD1(ListObjectAcl, ResponseWrapper<internal::ListObjectAclResponse>(
                                  internal::ListObjectAclRequest const&));
  MOCK_METHOD1(CreateObjectAcl, ResponseWrapper<ObjectAccessControl>(
                                    internal::CreateObjectAclRequest const&));
  MOCK_METHOD1(DeleteObjectAcl, ResponseWrapper<internal::EmptyResponse>(
                                    internal::ObjectAclRequest const&));
  MOCK_METHOD1(GetObjectAcl, ResponseWrapper<ObjectAccessControl>(
                                 internal::ObjectAclRequest const&));
  MOCK_METHOD1(UpdateObjectAcl, ResponseWrapper<ObjectAccessControl>(
                                    internal::UpdateObjectAclRequest const&));
};
}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_CLIENT_H_

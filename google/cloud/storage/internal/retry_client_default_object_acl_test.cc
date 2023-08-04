// Copyright 2023 Google LLC
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

#include "google/cloud/storage/internal/default_object_acl_requests.h"
#include "google/cloud/storage/internal/retry_client.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_generic_stub.h"
#include "google/cloud/storage/testing/retry_tests.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::storage::testing::MockGenericStub;
using ::google::cloud::storage::testing::RetryClientTestOptions;
using ::google::cloud::storage::testing::StoppedOnPermanentError;
using ::google::cloud::storage::testing::StoppedOnTooManyTransients;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::Return;

TEST(RetryClient, ListDefaultObjectAclTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListDefaultObjectAcl)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->ListDefaultObjectAcl(ListDefaultObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("ListDefaultObjectAcl"));
}

TEST(RetryClient, ListDefaultObjectAclPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListDefaultObjectAcl).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->ListDefaultObjectAcl(ListDefaultObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("ListDefaultObjectAcl"));
}

TEST(RetryClient, CreateDefaultObjectAclTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateDefaultObjectAcl)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->CreateDefaultObjectAcl(CreateDefaultObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("CreateDefaultObjectAcl"));
}

TEST(RetryClient, CreateDefaultObjectAclPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateDefaultObjectAcl).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->CreateDefaultObjectAcl(CreateDefaultObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("CreateDefaultObjectAcl"));
}

TEST(RetryClient, DeleteDefaultObjectAclTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteDefaultObjectAcl)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->DeleteDefaultObjectAcl(DeleteDefaultObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("DeleteDefaultObjectAcl"));
}

TEST(RetryClient, DeleteDefaultObjectAclPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteDefaultObjectAcl).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->DeleteDefaultObjectAcl(DeleteDefaultObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("DeleteDefaultObjectAcl"));
}

TEST(RetryClient, GetDefaultObjectAclTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetDefaultObjectAcl)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->GetDefaultObjectAcl(GetDefaultObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("GetDefaultObjectAcl"));
}

TEST(RetryClient, GetDefaultObjectAclPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetDefaultObjectAcl).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->GetDefaultObjectAcl(GetDefaultObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("GetDefaultObjectAcl"));
}

TEST(RetryClient, UpdateDefaultObjectAclTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateDefaultObjectAcl)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->UpdateDefaultObjectAcl(UpdateDefaultObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("UpdateDefaultObjectAcl"));
}

TEST(RetryClient, UpdateDefaultObjectAclPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateDefaultObjectAcl).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->UpdateDefaultObjectAcl(UpdateDefaultObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("UpdateDefaultObjectAcl"));
}

TEST(RetryClient, PatchDefaultObjectAclTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, PatchDefaultObjectAcl)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->PatchDefaultObjectAcl(PatchDefaultObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("PatchDefaultObjectAcl"));
}

TEST(RetryClient, PatchDefaultObjectAclPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, PatchDefaultObjectAcl).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->PatchDefaultObjectAcl(PatchDefaultObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("PatchDefaultObjectAcl"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

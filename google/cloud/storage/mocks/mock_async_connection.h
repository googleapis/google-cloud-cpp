// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_MOCKS_MOCK_ASYNC_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_MOCKS_MOCK_ASYNC_CONNECTION_H

#include "google/cloud/storage/async/connection.h"
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace storage_mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class MockAsyncConnection : public storage_experimental::AsyncConnection {
 public:
  MOCK_METHOD(Options, options, (), (const, override));
  MOCK_METHOD(future<StatusOr<storage::ObjectMetadata>>, InsertObject,
              (InsertObjectParams), (override));
  MOCK_METHOD(
      future<StatusOr<
          std::unique_ptr<storage_experimental::AsyncReaderConnection>>>,
      ReadObject, (ReadObjectParams), (override));
  MOCK_METHOD(future<StatusOr<storage_experimental::ReadPayload>>,
              ReadObjectRange, (ReadObjectParams), (override));
  MOCK_METHOD(
      future<StatusOr<
          std::unique_ptr<storage_experimental::AsyncWriterConnection>>>,
      StartUnbufferedUpload, (UploadParams), (override));
  MOCK_METHOD(
      future<StatusOr<
          std::unique_ptr<storage_experimental::AsyncWriterConnection>>>,
      StartBufferedUpload, (UploadParams), (override));
  MOCK_METHOD(
      future<StatusOr<
          std::unique_ptr<storage_experimental::AsyncWriterConnection>>>,
      ResumeUnbufferedUpload, (ResumeUploadParams), (override));
  MOCK_METHOD(
      future<StatusOr<
          std::unique_ptr<storage_experimental::AsyncWriterConnection>>>,
      ResumeBufferedUpload, (ResumeUploadParams), (override));
  MOCK_METHOD(future<StatusOr<google::storage::v2::Object>>, ComposeObject,
              (ComposeObjectParams), (override));
  MOCK_METHOD(future<Status>, DeleteObject, (DeleteObjectParams), (override));
  MOCK_METHOD(std::shared_ptr<storage_experimental::AsyncRewriterConnection>,
              RewriteObject, (RewriteObjectParams), (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_MOCKS_MOCK_ASYNC_CONNECTION_H

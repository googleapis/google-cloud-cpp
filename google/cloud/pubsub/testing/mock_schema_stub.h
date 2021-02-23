// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SCHEMA_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SCHEMA_STUB_H

#include "google/cloud/pubsub/internal/schema_stub.h"
#include "google/cloud/pubsub/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_testing {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * A class to mock pubsub_internal::SchemaStub
 */
class MockSchemaStub : public pubsub_internal::SchemaStub {
 public:
  ~MockSchemaStub() override = default;

  MOCK_METHOD(StatusOr<google::pubsub::v1::Schema>, CreateSchema,
              (grpc::ClientContext&,
               google::pubsub::v1::CreateSchemaRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::pubsub::v1::Schema>, GetSchema,
              (grpc::ClientContext&,
               google::pubsub::v1::GetSchemaRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::pubsub::v1::ListSchemasResponse>, ListSchemas,
              (grpc::ClientContext&,
               google::pubsub::v1::ListSchemasRequest const&),
              (override));
  MOCK_METHOD(Status, DeleteSchema,
              (grpc::ClientContext&,
               google::pubsub::v1::DeleteSchemaRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::pubsub::v1::ValidateSchemaResponse>,
              ValidateSchema,
              (grpc::ClientContext&,
               google::pubsub::v1::ValidateSchemaRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::pubsub::v1::ValidateMessageResponse>,
              ValidateMessage,
              (grpc::ClientContext&,
               google::pubsub::v1::ValidateMessageRequest const&),
              (override));
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SCHEMA_STUB_H

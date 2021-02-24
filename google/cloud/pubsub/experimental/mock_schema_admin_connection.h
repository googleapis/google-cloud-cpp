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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_EXPERIMENTAL_MOCK_SCHEMA_ADMIN_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_EXPERIMENTAL_MOCK_SCHEMA_ADMIN_CONNECTION_H

#include "google/cloud/pubsub/experimental/schema_admin_connection.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_experimental {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * A googlemock-based mock for
 * [pubsub_experimental::SchemaAdminConnection][mocked-link]
 *
 * [mocked-link]: @ref
 * google::cloud::pubsub_experimental::v1::SchemaAdminConnection
 */
class MockSchemaAdminConnection
    : public pubsub_experimental::SchemaAdminConnection {
 public:
  MOCK_METHOD(StatusOr<google::pubsub::v1::Schema>, CreateSchema,
              (google::pubsub::v1::CreateSchemaRequest const&), (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Schema>, GetSchema,
              (google::pubsub::v1::GetSchemaRequest const&), (override));

  MOCK_METHOD(ListSchemasRange, ListSchemas,
              (google::pubsub::v1::ListSchemasRequest const&), (override));

  MOCK_METHOD(Status, DeleteSchema,
              (google::pubsub::v1::DeleteSchemaRequest const&), (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::ValidateSchemaResponse>,
              ValidateSchema,
              (google::pubsub::v1::ValidateSchemaRequest const&), (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::ValidateMessageResponse>,
              ValidateMessage,
              (google::pubsub::v1::ValidateMessageRequest const&), (override));
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_EXPERIMENTAL_MOCK_SCHEMA_ADMIN_CONNECTION_H

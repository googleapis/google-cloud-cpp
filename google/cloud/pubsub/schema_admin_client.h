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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SCHEMA_ADMIN_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SCHEMA_ADMIN_CLIENT_H

#include "google/cloud/pubsub/schema.h"
#include "google/cloud/pubsub/schema_admin_connection.h"
#include "google/cloud/pubsub/version.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Performs schema admin operations in Cloud Pub/Sub.
 *
 * Applications use this class to perform operations on
 * [Cloud Pub/Sub][pubsub-doc-link].
 *
 * @warning The Cloud Pub/Sub schema API and the C++ client library for the
 *     Cloud Pub/Sub schema APIs are experimental. They are subject to change,
 *     including complete removal, without notice.
 *
 * @par Performance
 * `SchemaAdminClient` objects are cheap to create, copy, and move. However,
 * each `SchemaAdminClient` object must be created with a
 * `std::shared_ptr<SchemaAdminConnection>`, which itself is relatively
 * expensive to create. Therefore, connection instances should be shared when
 * possible. See the `MakeSchemaAdminConnection()` function and the
 * `SchemaAdminConnection` interface for more details.
 *
 * @par Thread Safety
 * Instances of this class created via copy-construction or copy-assignment
 * share the underlying pool of connections. Access to these copies via multiple
 * threads is guaranteed to work. Two threads operating on the same instance of
 * this class is not guaranteed to work.
 *
 * @par Error Handling
 * This class uses `StatusOr<T>` to report errors. When an operation fails to
 * perform its work the returned `StatusOr<T>` contains the error details. If
 * the `ok()` member function in the `StatusOr<T>` returns `true` then it
 * contains the expected result. Please consult the [`StatusOr<T>`
 * documentation](#google::cloud::StatusOr) for more details.
 *
 * [pubsub-doc-link]: https://cloud.google.com/pubsub/docs
 */
class SchemaAdminClient {
 public:
  explicit SchemaAdminClient(std::shared_ptr<SchemaAdminConnection> connection,
                             Options opts = {});

  /**
   * The default constructor is deleted.
   *
   * Use `PublisherClient(std::shared_ptr<PublisherConnection>)`
   */
  SchemaAdminClient() = delete;

  /**
   * @copydoc CreateSchema(google::pubsub::v1::CreateSchemaRequest const&,Options)
   *
   * @par Example
   * @snippet samples.cc create-avro-schema
   */
  StatusOr<google::pubsub::v1::Schema> CreateAvroSchema(
      Schema const& schema, std::string schema_definition, Options opts = {});

  /**
   * @copydoc CreateSchema(google::pubsub::v1::CreateSchemaRequest const&,Options)
   *
   * @par Example
   * @snippet samples.cc create-protobuf-schema
   */
  StatusOr<google::pubsub::v1::Schema> CreateProtobufSchema(
      Schema const& schema, std::string schema_definition, Options opts = {});

  /**
   * Creates a new Cloud Pub/Sub schema.
   *
   * @par Idempotency
   * This operation is idempotent, as it succeeds only once, therefore the
   * library retries the call. It might return a status code of`kAlreadyExists`
   * as a consequence of retrying a successful (but reported as failed) request.
   */
  StatusOr<google::pubsub::v1::Schema> CreateSchema(
      google::pubsub::v1::CreateSchemaRequest const& request,
      Options opts = {});

  /**
   * Gets information about an existing Cloud Pub/Sub schema.
   *
   * @par Idempotency
   * This is a read-only operation and therefore always idempotent and retried.
   *
   * @par Example
   * @snippet samples.cc get-schema
   *
   * @param schema the full name of the schema
   * @param view Use `BASIC` to include the name and type of the schema, but not
   *     the definition. Use `FULL` to include the definition.
   * @param opts Override the class-level options, such as retry and backoff
   *     policies.
   */
  StatusOr<google::pubsub::v1::Schema> GetSchema(
      Schema const& schema,
      google::pubsub::v1::SchemaView view = google::pubsub::v1::BASIC,
      Options opts = {});

  /**
   * Lists all the schemas for a given project id.
   *
   * @par Idempotency
   * This is a read-only operation and therefore always idempotent and retried.
   *
   * @par Example
   * @snippet samples.cc list-schemas
   *
   * @param project_id lists the schemas in this project
   * @param view Use `BASIC` to include the name and type of each schema, but
   *     not the definition. Use `FULL` to include the definition.
   * @param opts Override the class-level options, such as retry and backoff
   *     policies.
   */
  ListSchemasRange ListSchemas(
      std::string const& project_id,
      google::pubsub::v1::SchemaView view = google::pubsub::v1::BASIC,
      Options opts = {});

  /**
   * Deletes an existing schema in Cloud Pub/Sub.
   *
   * @par Idempotency
   * This operation is idempotent, the state of the system is the same after one
   * or several calls, and therefore it is always retried. It might return a
   * status code of `kNotFound` as a consequence of retrying a successful
   * (but reported as failed) request.
   *
   * @par Example
   * @snippet samples.cc delete-schema
   *
   * @param schema the name of the schema to be deleted.
   * @param opts Override the class-level options, such as retry and backoff
   *     policies.
   */
  Status DeleteSchema(Schema const& schema, Options opts = {});

  /**
   * Validates a schema definition.
   *
   * @par Idempotency
   * This is a read-only operation and therefore always idempotent and retried.
   *
   * @par Example
   * @snippet samples.cc validate-avro-schema
   */
  StatusOr<google::pubsub::v1::ValidateSchemaResponse> ValidateAvroSchema(
      std::string const& project_id, std::string schema_definition,
      Options opts = {});

  /**
   * Validates a schema definition.
   *
   * @par Idempotency
   * This is a read-only operation and therefore always idempotent and retried.
   *
   * @par Example
   * @snippet samples.cc validate-protobuf-schema
   */
  StatusOr<google::pubsub::v1::ValidateSchemaResponse> ValidateProtobufSchema(
      std::string const& project_id, std::string schema_definition,
      Options opts = {});

  /**
   * Validates a schema definition.
   *
   * @par Idempotency
   * This is a read-only operation and therefore always idempotent and retried.
   */
  StatusOr<google::pubsub::v1::ValidateSchemaResponse> ValidateSchema(
      std::string const& project_id, google::pubsub::v1::Schema schema,
      Options opts = {});

  /**
   * @copydoc ValidateMessage(google::pubsub::v1::ValidateMessageRequest const&,Options)
   *
   * @par Example
   * @snippet samples.cc validate-message-named-schema
   *
   * @param encoding the message encoding, note that some schemas may not
   * support some encodings.
   * @param message the message to validate
   * @param named_schema the name of an existing schema to validate against
   * @param opts Override the class-level options, such as retry and backoff
   *     policies.
   */
  StatusOr<google::pubsub::v1::ValidateMessageResponse>
  ValidateMessageWithNamedSchema(google::pubsub::v1::Encoding encoding,
                                 std::string message,
                                 Schema const& named_schema, Options opts = {});

  /**
   * @copydoc ValidateMessage(google::pubsub::v1::ValidateMessageRequest const&,Options)
   *
   * @par Example
   * @snippet samples.cc validate-message-avro
   *
   * @param encoding the message encoding, note that some schemas may not
   * support some encodings.
   * @param message the message to validate
   * @param project_id the project used to perform the validation
   * @param schema_definition the schema definition, in AVRO format
   * @param opts Override the class-level options, such as retry and backoff
   *     policies.
   */
  StatusOr<google::pubsub::v1::ValidateMessageResponse> ValidateMessageWithAvro(
      google::pubsub::v1::Encoding encoding, std::string message,
      std::string project_id, std::string schema_definition, Options opts = {});

  /**
   * @copydoc ValidateMessage(google::pubsub::v1::ValidateMessageRequest const&,Options)
   *
   * @par Example
   * @snippet samples.cc validate-message-protobuf
   *
   * @param encoding the message encoding, note that some schemas may not
   *     support some encodings.
   * @param message the message to validate
   * @param project_id the project used to perform the validation
   * @param schema_definition the schema definition, in protocol buffers format
   * @param opts Override the class-level options, such as retry and backoff
   *     policies.
   */
  StatusOr<google::pubsub::v1::ValidateMessageResponse>
  ValidateMessageWithProtobuf(google::pubsub::v1::Encoding encoding,
                              std::string message, std::string project_id,
                              std::string schema_definition, Options opts = {});

  /**
   * Validates a message against a schema.
   *
   * @par Idempotency
   * This is a read-only operation and therefore always idempotent and retried.
   */
  StatusOr<google::pubsub::v1::ValidateMessageResponse> ValidateMessage(
      google::pubsub::v1::ValidateMessageRequest const& request,
      Options opts = {});

 private:
  std::shared_ptr<SchemaAdminConnection> connection_;
  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SCHEMA_ADMIN_CLIENT_H

// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_PUBSUB_GOOGLE_CLOUD_PUBSUB_PUBLISHER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_PUBSUB_GOOGLE_CLOUD_PUBSUB_PUBLISHER_CONNECTION_H

#include "google/cloud/pubsub/connection_options.h"
#include "google/cloud/status_or.h"
#include <google/pubsub/v1/pubsub.pb.h>
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * A connection to Cloud Pub/Sub.
 *
 * This interface defines pure-virtual methods for each of the user-facing
 * overload sets in `PublisherClient`. That is, all of `PublisherClient`'s
 * `Publish()` overloads will forward to the one pure-virtual `Publish()` method
 * declared in this interface, and similar for `PublisherClient`'s other
 * methods. This allows users to inject custom behavior (e.g., with a Google
 * Mock object) in a `PublisherClient` object for use in their own tests.
 *
 * To create a concrete instance that connects you to the real Cloud Pub/Sub
 * service, see `MakePublisherConnection()`.
 */
class PublisherConnection {
 public:
  virtual ~PublisherConnection() = 0;

  //@{
  /**
   * Define the arguments for each member function.
   *
   * Applications may define classes derived from `PublisherConnection`, for
   * example, because they want to mock the class. To avoid breaking all such
   * derived classes when we change the number or type of the arguments to the
   * member functions we define light weight structures to pass the arguments.
   */
  /// Wrap the arguments for `CreateTopic()`
  struct CreateTopicParams {
    std::string project_id;
    std::string topic_id;
    std::map<std::string, std::string> labels;
    std::vector<std::string> allowed_persistent_regions;
    std::string kms_key_name;
  };

  /// Wrap the arguments for `DeleteTopic()`
  struct DeleteTopicParams {
    std::string project_id;
    std::string topic_id;
  };
  //@}

  /// Defines the interface for `Client::CreateTopic()`
  virtual StatusOr<google::pubsub::v1::Topic> CreateTopic(
      CreateTopicParams) = 0;

  /// Defines the interface for `Client::DeleteTopic()`
  virtual Status DeleteTopic(DeleteTopicParams) = 0;
};

/**
 * Returns an PublisherConnection object to work with Cloud Pub/Sub publisher
 * APIs.
 *
 * The `PublisherConnection` class is not intended for direct use in
 * applications, it is provided for applications wanting to mock the
 * `PublisherClient` behavior in their tests.
 *
 * @see `PublisherConnection`
 *
 * @param options (optional) configure the `PublisherConnection` created by
 *     this function.
 */
std::shared_ptr<PublisherConnection> MakePublisherConnection(
    ConnectionOptions const& options = ConnectionOptions());

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_PUBSUB_GOOGLE_CLOUD_PUBSUB_PUBLISHER_CONNECTION_H

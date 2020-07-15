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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIPTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIPTION_H

#include "google/cloud/pubsub/version.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Objects of this class identify a Cloud Pub/Sub subscription.
 *
 * @note
 * This class makes no effort to validate the ids provided. The application
 * should verify that any ids passed to this application conform to the
 * Cloud Pub/Sub [resource name][name-link] restrictions.
 *
 * [name-link]: https://cloud.google.com/pubsub/docs/admin#resource_names
 */
class Subscription {
 public:
  Subscription(std::string project_id, std::string subscription_id)
      : project_id_(std::move(project_id)),
        subscription_id_(std::move(subscription_id)) {}

  /// @name Copy and move
  //@{
  Subscription(Subscription const&) = default;
  Subscription& operator=(Subscription const&) = default;
  Subscription(Subscription&&) = default;
  Subscription& operator=(Subscription&&) = default;
  //@}

  /// Returns the Project ID
  std::string const& project_id() const { return project_id_; }

  /// Returns the Subscription ID
  std::string const& subscription_id() const { return subscription_id_; }

  /**
   * Returns the fully qualified subscription name as a string of the form:
   * "projects/<project-id>/subscriptions/<subscription-id>"
   */
  std::string FullName() const;

  /// @name Equality operators
  //@{
  friend bool operator==(Subscription const& a, Subscription const& b);
  friend bool operator!=(Subscription const& a, Subscription const& b) {
    return !(a == b);
  }
  //@}

  /// Output the `FullName()` format.
  friend std::ostream& operator<<(std::ostream& os, Subscription const& rhs);

 private:
  std::string project_id_;
  std::string subscription_id_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIPTION_H

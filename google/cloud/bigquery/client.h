// Copyright 2019 Google LLC
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

#ifndef BIGQUERY_CLIENT_H_
#define BIGQUERY_CLIENT_H_

#include <memory>

#include "google/cloud/bigquery/connection.h"
#include "google/cloud/bigquery/connection_options.h"
#include "google/cloud/bigquery/version.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace bigquery {
inline namespace BIGQUERY_CLIENT_NS {
class Client {
 public:
  explicit Client(std::shared_ptr<Connection> conn) : conn_(std::move(conn)) {}

  Client() = delete;

  // Client is copyable and movable.
  Client(Client const&) = default;
  Client& operator=(Client const&) = default;
  Client(Client&&) = default;
  Client& operator=(Client&&) = default;

  friend bool operator==(Client const& a, Client const& b) {
    return a.conn_ == b.conn_;
  }

  friend bool operator!=(Client const& a, Client const& b) { return !(a == b); }

  // Creates a new read session and returns its name if successful.
  //
  // This function is just a proof of concept to ensure we can send
  // requests to the server.
  google::cloud::StatusOr<std::string> CreateSession(
      std::string parent_project_id, std::string table);

 private:
  std::shared_ptr<Connection> conn_;
};

std::shared_ptr<Connection> MakeConnection(ConnectionOptions const& options);

}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google

#endif  // BIGQUERY_CLIENT_H_

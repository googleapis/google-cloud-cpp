// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_EMBEDDED_SERVER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_EMBEDDED_SERVER_H

#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
namespace benchmarks {
/**
 * An abstract class to run and stop the embedded Bigtable server.
 *
 * Sometimes it is interesting to run performance benchmarks against an
 * embedded server, as this eliminates sources of variation when measuring
 * small changes to the library.  This class is used to run (using Wait()) and
 * stop (using Shutdown()) such a server, without exposing the implementation
 * details to the application.
 */
class EmbeddedServer {
 public:
  virtual ~EmbeddedServer() = default;

  virtual std::string address() const = 0;
  virtual void Shutdown() = 0;
  virtual void Wait() = 0;

  virtual int create_table_count() const = 0;
  virtual int delete_table_count() const = 0;
  virtual int mutate_row_count() const = 0;
  virtual int mutate_rows_count() const = 0;
  virtual int read_rows_count() const = 0;
};

/// Create an embedded server.
std::unique_ptr<EmbeddedServer> CreateEmbeddedServer();

}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_EMBEDDED_SERVER_H

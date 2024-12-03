// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_SERVER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_SERVER_H

#include <cstdint>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

class EmulatorServer {
 public:
  virtual ~EmulatorServer() = default;

  virtual int bound_port() = 0;
  virtual void Shutdown() = 0;
  virtual void Wait() = 0;
};

std::unique_ptr<EmulatorServer> CreateDefaultEmulatorServer(
    std::string const& host, std::uint16_t port);

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_SERVER_H

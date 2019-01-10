// Copyright 2018 Google Inc.
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

#include "google/cloud/bigtable/internal/grpc_error_delegate.h"
#include "google/cloud/bigtable/grpc_error.h"
#include "google/cloud/terminate_handler.h"
#include <sstream>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
void RaiseRpcError(grpc::Status const& status, char const* msg) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  throw bigtable::GRpcError(msg, status);
#else
  bigtable::GRpcError ex(msg, status);
  std::cerr << "Aborting because exceptions are disabled: " << ex.what()
            << std::endl;
  google::cloud::Terminate(ex.what());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

void RaiseRpcError(grpc::Status const& status, std::string const& msg) {
  RaiseRpcError(status, msg.c_str());
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

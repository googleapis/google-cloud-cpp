// Copyright 2020 Google LLC
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

#include "generator/internal/predicate_utils.h"
#include "generator/internal/descriptor_utils.h"
#include "google/cloud/log.h"
#include "google/longrunning/operations.pb.h"
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {

using ::google::protobuf::MethodDescriptor;

bool IsStreaming(MethodDescriptor const& method) {
  return method.client_streaming() || method.server_streaming();
}

bool IsNonStreaming(MethodDescriptor const& method) {
  return !IsStreaming(method);
}

bool IsStreamingRead(MethodDescriptor const& method) {
  return !method.client_streaming() && method.server_streaming();
}

bool IsStreamingWrite(MethodDescriptor const& method) {
  return method.client_streaming() && !method.server_streaming();
}

bool IsBidirStreaming(MethodDescriptor const& method) {
  return method.client_streaming() && method.server_streaming();
}

bool IsResponseTypeEmpty(MethodDescriptor const& method) {
  return method.output_type()->full_name() == "google.protobuf.Empty";
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

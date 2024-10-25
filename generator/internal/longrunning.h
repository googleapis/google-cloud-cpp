// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_LONGRUNNING_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_LONGRUNNING_H

#include "generator/internal/printer.h"
#include "google/protobuf/descriptor.h"

namespace google {
namespace cloud {
namespace generator_internal {

/**
 * Determines if the given method is a long running operation.
 */
bool IsLongrunningOperation(google::protobuf::MethodDescriptor const& method);

/**
 * Determines if the given method's response is contained in the longrunning
 * metadata field.
 */
bool IsLongrunningMetadataTypeUsedAsResponse(
    google::protobuf::MethodDescriptor const& method);

/**
 * Sets longrunning operation related key/value pairs in method_vars.
 */
void SetLongrunningOperationMethodVars(
    google::protobuf::MethodDescriptor const& method,
    VarsDictionary& method_vars);

/**
 * Determines if the method uses `google::longrunning::Operation` types for
 * long running operations.
 */
bool IsGRPCLongrunningOperation(
    google::protobuf::MethodDescriptor const& method);

/**
 * Determines if the method definition contains the
 * `google::cloud::operation_service` extension which defines the endpoint to
 * poll.
 */
bool IsHttpLongrunningOperation(
    google::protobuf::MethodDescriptor const& method);

/**
 * Sets longrunning operation related key/value pairs in service_vars.
 */
void SetLongrunningOperationServiceVars(
    google::protobuf::ServiceDescriptor const& service,
    VarsDictionary& service_vars);

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_LONGRUNNING_H

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

#include "generator/internal/stub_generator_base.h"
#include "generator/internal/longrunning.h"
#include "generator/internal/predicate_utils.h"
#include <google/protobuf/descriptor.h>

namespace google {
namespace cloud {
namespace generator_internal {

StubGeneratorBase::StubGeneratorBase(
    std::string const& header_path_key, std::string const& cc_path_key,
    google::protobuf::ServiceDescriptor const* service_descriptor,
    VarsDictionary service_vars,
    std::map<std::string, VarsDictionary> service_method_vars,
    google::protobuf::compiler::GeneratorContext* context)
    : ServiceCodeGenerator(header_path_key, cc_path_key, service_descriptor,
                           std::move(service_vars),
                           std::move(service_method_vars), context) {}

void StubGeneratorBase::HeaderPrintPublicMethods() {
  for (auto const& method : methods()) {
    if (IsStreamingWrite(method)) {
      HeaderPrintMethod(method, __FILE__, __LINE__, R"""(
  std::unique_ptr<::google::cloud::internal::StreamingWriteRpc<
      $request_type$,
      $response_type$>>
  $method_name$(
      std::shared_ptr<grpc::ClientContext> context,
      Options const& options) override;
)""");
      continue;
    }
    if (IsBidirStreaming(method)) {
      HeaderPrintMethod(method, __FILE__, __LINE__, R"""(
  std::unique_ptr<::google::cloud::AsyncStreamingReadWriteRpc<
      $request_type$,
      $response_type$>>
  Async$method_name$(
      google::cloud::CompletionQueue const& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options) override;
)""");
      continue;
    }
    if (IsLongrunningOperation(method)) {
      HeaderPrintMethod(method, __FILE__, __LINE__, R"""(
  future<StatusOr<google::longrunning::Operation>> Async$method_name$(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      $request_type$ const& request) override;
)""");
      HeaderPrintMethod(method, __FILE__, __LINE__, R"""(
  StatusOr<google::longrunning::Operation> $method_name$(
      grpc::ClientContext& context,
      Options options,
      $request_type$ const& request) override;
)""");
      continue;
    }
    if (IsStreamingRead(method)) {
      HeaderPrintMethod(method, __FILE__, __LINE__, R"""(
  std::unique_ptr<google::cloud::internal::StreamingReadRpc<$response_type$>>
  $method_name$(
      std::shared_ptr<grpc::ClientContext> context,
      Options const& options,
      $request_type$ const& request) override;
)""");
      continue;
    }
    if (IsResponseTypeEmpty(method)) {
      HeaderPrintMethod(method, __FILE__, __LINE__, R"""(
  Status $method_name$(
      grpc::ClientContext& context,
      Options const& options,
      $request_type$ const& request) override;
)""");
      continue;
    }
    HeaderPrintMethod(method, __FILE__, __LINE__, R"""(
  StatusOr<$response_type$> $method_name$(
      grpc::ClientContext& context,
      Options const& options,
      $request_type$ const& request) override;
)""");
  }

  for (auto const& method : async_methods()) {
    // Nothing to do, these are always asynchronous.
    if (IsBidirStreaming(method) || IsLongrunningOperation(method)) continue;
    if (IsStreamingRead(method)) {
      auto constexpr kDeclaration = R"""(
  std::unique_ptr<::google::cloud::internal::AsyncStreamingReadRpc<
      $response_type$>>
  Async$method_name$(
      google::cloud::CompletionQueue const& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      $request_type$ const& request) override;
)""";
      HeaderPrintMethod(method, __FILE__, __LINE__, kDeclaration);
      continue;
    }
    if (IsStreamingWrite(method)) {
      auto constexpr kDeclaration = R"""(
  std::unique_ptr<::google::cloud::internal::AsyncStreamingWriteRpc<
      $request_type$, $response_type$>>
  Async$method_name$(
      google::cloud::CompletionQueue const& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options) override;
)""";
      HeaderPrintMethod(method, __FILE__, __LINE__, kDeclaration);
      continue;
    }
    if (IsResponseTypeEmpty(method)) {
      HeaderPrintMethod(method, __FILE__, __LINE__, R"""(
  future<Status> Async$method_name$(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      $request_type$ const& request) override;
)""");
      continue;
    }
    HeaderPrintMethod(method, __FILE__, __LINE__, R"""(
  future<StatusOr<$response_type$>> Async$method_name$(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      $request_type$ const& request) override;
)""");
  }

  if (HasLongrunningMethod()) {
    HeaderPrint(R"""(
  future<StatusOr<google::longrunning::Operation>> AsyncGetOperation(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::longrunning::GetOperationRequest const& request) override;

  future<Status> AsyncCancelOperation(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::longrunning::CancelOperationRequest const& request) override;
)""");
  }
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

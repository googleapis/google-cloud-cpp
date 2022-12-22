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
      $response_type$>> $method_name$(
      std::unique_ptr<grpc::ClientContext> context) override;
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
      std::unique_ptr<grpc::ClientContext> context) override;
)""");
      continue;
    }
    HeaderPrintMethod(
        method,
        {MethodPattern({{IsResponseTypeEmpty,
                         R"""(
  Status $method_name$(
      grpc::ClientContext& context,
      $request_type$ const& request) override;
)""",
                         R"""(
  StatusOr<$response_type$> $method_name$(
      grpc::ClientContext& context,
      $request_type$ const& request) override;
)"""},
                        {""}},
                       And(IsNonStreaming, Not(IsLongrunningOperation))),
         MethodPattern({{R"""(
  future<StatusOr<google::longrunning::Operation>> Async$method_name$(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      $request_type$ const& request) override;
)"""}},
                       IsLongrunningOperation),
         MethodPattern({{R"""(
  std::unique_ptr<google::cloud::internal::StreamingReadRpc<$response_type$>>
  $method_name$(
      std::unique_ptr<grpc::ClientContext> context,
      $request_type$ const& request) override;
)"""}},
                       IsStreamingRead)},
        __FILE__, __LINE__);
  }

  for (auto const& method : async_methods()) {
    if (IsStreamingRead(method)) {
      auto constexpr kDeclaration = R"""(
  std::unique_ptr<::google::cloud::internal::AsyncStreamingReadRpc<
      $response_type$>>
  Async$method_name$(
      google::cloud::CompletionQueue const& cq,
      std::unique_ptr<grpc::ClientContext> context,
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
      std::unique_ptr<grpc::ClientContext> context) override;
)""";
      HeaderPrintMethod(method, __FILE__, __LINE__, kDeclaration);
      continue;
    }
    HeaderPrintMethod(
        method,
        {MethodPattern({{IsResponseTypeEmpty,
                         R"""(
  future<Status> Async$method_name$(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      $request_type$ const& request) override;
)""",
                         R"""(
  future<StatusOr<$response_type$>> Async$method_name$(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      $request_type$ const& request) override;
)"""},
                        {""}},
                       And(IsNonStreaming, Not(IsLongrunningOperation)))},
        __FILE__, __LINE__);
  }

  if (HasLongrunningMethod()) {
    HeaderPrint(R"""(
  future<StatusOr<google::longrunning::Operation>> AsyncGetOperation(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::longrunning::GetOperationRequest const& request) override;

  future<Status> AsyncCancelOperation(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::longrunning::CancelOperationRequest const& request) override;
)""");
  }
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

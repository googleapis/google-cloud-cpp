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

#include "generator/internal/longrunning.h"
#include "generator/internal/codegen_utils.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "absl/types/variant.h"
#include <google/cloud/extended_operations.pb.h>
#include <google/longrunning/operations.pb.h>
#include <string>

using ::google::protobuf::Descriptor;
using ::google::protobuf::MethodDescriptor;
using ::google::protobuf::SourceLocation;

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

absl::variant<std::string, Descriptor const*> FullyQualifyMessageType(
    MethodDescriptor const& method, std::string message_type) {
  Descriptor const* output_type =
      method.file()->pool()->FindMessageTypeByName(message_type);
  if (output_type != nullptr) {
    return output_type;
  }
  output_type = method.file()->pool()->FindMessageTypeByName(
      absl::StrCat(method.file()->package(), ".", message_type));
  if (output_type != nullptr) {
    return output_type;
  }
  return message_type;
}

struct FullyQualifiedMessageTypeVisitor {
  std::string operator()(std::string const& s) const { return s; }
  std::string operator()(Descriptor const* d) const { return d->full_name(); }
};

// TODO(#11545): once this function exists in a header outside of
// descriptor_utils.h, include the new header instead and remove this
// implementation.
std::string FormatDoxygenLink(Descriptor const& message_type) {
  SourceLocation loc;
  message_type.GetSourceLocation(&loc);
  std::string output_type_proto_file_name = message_type.file()->name();
  return absl::StrCat(
      "@googleapis_link{", ProtoNameToCppName(message_type.full_name()), ",",
      output_type_proto_file_name, "#L", loc.start_line + 1, "}");
}

struct FormatDoxygenLinkVisitor {
  explicit FormatDoxygenLinkVisitor() = default;
  std::string operator()(std::string const& s) const {
    return ProtoNameToCppName(s);
  }
  std::string operator()(google::protobuf::Descriptor const* d) const {
    return FormatDoxygenLink(*d);
  }
};

absl::variant<std::string, Descriptor const*>
DeduceLongrunningOperationResponseType(
    MethodDescriptor const& method,
    google::longrunning::OperationInfo const& operation_info) {
  std::string deduced_response_type =
      operation_info.response_type() == "google.protobuf.Empty"
          ? operation_info.metadata_type()
          : operation_info.response_type();
  return FullyQualifyMessageType(method, deduced_response_type);
}

}  // namespace

bool IsLongrunningOperation(MethodDescriptor const& method) {
  bool grpc_lro =
      method.output_type()->full_name() == "google.longrunning.Operation";
  auto operation_service_extension =
      method.options().GetExtension(google::cloud::operation_service);
  bool http_lro = !operation_service_extension.empty();
  return grpc_lro || http_lro;
}

bool IsLongrunningMetadataTypeUsedAsResponse(MethodDescriptor const& method) {
  if (method.output_type()->full_name() == "google.longrunning.Operation") {
    auto operation_info =
        method.options().GetExtension(google::longrunning::operation_info);
    return operation_info.response_type() == "google.protobuf.Empty";
  }
  return false;
}

void SetLongrunningOperationMethodVars(
    google::protobuf::MethodDescriptor const& method,
    VarsDictionary& method_vars) {
  if (method.output_type()->full_name() == "google.longrunning.Operation") {
    auto operation_info =
        method.options().GetExtension(google::longrunning::operation_info);
    method_vars["longrunning_metadata_type"] = ProtoNameToCppName(absl::visit(
        FullyQualifiedMessageTypeVisitor(),
        FullyQualifyMessageType(method, operation_info.metadata_type())));
    method_vars["longrunning_response_type"] = ProtoNameToCppName(absl::visit(
        FullyQualifiedMessageTypeVisitor(),
        FullyQualifyMessageType(method, operation_info.response_type())));
    auto deduced_response_type =
        DeduceLongrunningOperationResponseType(method, operation_info);
    method_vars["longrunning_deduced_response_message_type"] =
        absl::visit(FullyQualifiedMessageTypeVisitor(), deduced_response_type);
    method_vars["longrunning_deduced_response_type"] = ProtoNameToCppName(
        method_vars["longrunning_deduced_response_message_type"]);
    method_vars["method_longrunning_deduced_return_doxygen_link"] =
        absl::visit(FormatDoxygenLinkVisitor{}, deduced_response_type);
    return;
  }

  auto operation_service_extension =
      method.options().GetExtension(google::cloud::operation_service);
  if (!operation_service_extension.empty()) {
    method_vars["longrunning_response_type"] = ProtoNameToCppName(absl::visit(
        FullyQualifiedMessageTypeVisitor(),
        FullyQualifyMessageType(method, method.output_type()->full_name())));
    absl::variant<std::string, google::protobuf::Descriptor const*>
        deduced_response_type = method.output_type();
    method_vars["longrunning_deduced_response_message_type"] =
        absl::visit(FullyQualifiedMessageTypeVisitor(), deduced_response_type);
    method_vars["longrunning_deduced_response_type"] = ProtoNameToCppName(
        method_vars["longrunning_deduced_response_message_type"]);
    method_vars["method_longrunning_deduced_return_doxygen_link"] =
        absl::visit(FormatDoxygenLinkVisitor{}, deduced_response_type);
  }
}

bool IsGRPCLongrunningOperation(MethodDescriptor const& method) {
  return method.output_type()->full_name() == "google.longrunning.Operation";
}

bool IsHttpLongrunningOperation(MethodDescriptor const& method) {
  auto operation_service_extension =
      method.options().GetExtension(google::cloud::operation_service);
  return !operation_service_extension.empty();
}

void SetLongrunningOperationServiceVars(
    google::protobuf::ServiceDescriptor const& service,
    VarsDictionary& service_vars) {
  for (int i = 0; i != service.method_count(); ++i) {
    auto const* method = service.method(i);
    if (method->output_type()->full_name() == "google.longrunning.Operation") {
      service_vars["longrunning_operation_include_header"] =
          "google/longrunning/operations.pb.h";
      service_vars["longrunning_response_type"] =
          "google::longrunning::Operation";
      service_vars["longrunning_get_operation_request_type"] =
          "google::longrunning::GetOperationRequest";
      service_vars["longrunning_cancel_operation_request_type"] =
          "google::longrunning::CancelOperationRequest";
      service_vars["longrunning_get_operation_path"] =
          R"""(absl::StrCat("/v1/", request.name()))""";
      service_vars["longrunning_cancel_operation_path"] =
          R"""(absl::StrCat("/v1/", request.name(), ":cancel"))""";
      return;
    }
    if (!method->options()
             .GetExtension(google::cloud::operation_service)
             .empty()) {
      service_vars["longrunning_response_type"] = ProtoNameToCppName(
          absl::visit(FullyQualifiedMessageTypeVisitor(),
                      FullyQualifyMessageType(
                          *method, method->output_type()->full_name())));
      auto operation_service_extension =
          method->options().GetExtension(google::cloud::operation_service);
      if (operation_service_extension == "GlobalOperations") {
        service_vars["longrunning_operation_include_header"] =
            "google/cloud/compute/global_operations/v1/global_operations.pb.h";
        service_vars["longrunning_get_operation_request_type"] =
            "google::cloud::cpp::compute::global_operations::v1::"
            "GetGlobalOperationRequest";
        service_vars["longrunning_cancel_operation_request_type"] =
            "google::cloud::cpp::compute::global_operations::v1::"
            "DeleteGlobalOperationRequest";
        service_vars["longrunning_set_operation_fields"] = R"""(
      r.set_project(request.project());
      r.set_operation(op);
)""";
        service_vars["longrunning_get_operation_path"] =
            R"""(absl::StrCat("/compute/v1/projects/", request.project(),
                             "/global/operations/",
                             request.operation()))""";
        service_vars["longrunning_cancel_operation_path"] =
            R"""(absl::StrCat("/compute/v1/projects/", request.project(),
                             "/global/operations/",
                             request.operation()))""";
      } else if (operation_service_extension ==
                 "GlobalOrganizationOperations") {
        service_vars["longrunning_operation_include_header"] =
            "google/cloud/compute/global_organization_operations/v1/"
            "global_organization_operations.pb.h";
        service_vars["longrunning_get_operation_request_type"] =
            "google::cloud::cpp::compute::global_organization_operations::v1::"
            "GetGlobalOrganizationOperationRequest";
        service_vars["longrunning_cancel_operation_request_type"] =
            "google::cloud::cpp::compute::global_organization_operations::v1::"
            "DeleteGlobalOrganizationOperationRequest";
        service_vars["longrunning_set_operation_fields"] = R"""(
      r.set_operation(op);
)""";
        service_vars["longrunning_get_operation_path"] =
            R"""(absl::StrCat("/compute/v1/locations/global/operations/",
                             request.operation()))""";
        service_vars["longrunning_cancel_operation_path"] =
            R"""(absl::StrCat("/compute/v1/locations/global/operations/",
                             request.operation()))""";
      } else if (operation_service_extension == "RegionOperations") {
        service_vars["longrunning_operation_include_header"] =
            "google/cloud/compute/region_operations/v1/region_operations.pb.h";
        service_vars["longrunning_get_operation_request_type"] =
            "google::cloud::cpp::compute::region_operations::v1::"
            "GetRegionOperationRequest";
        service_vars["longrunning_cancel_operation_request_type"] =
            "google::cloud::cpp::compute::region_operations::v1::"
            "DeleteRegionOperationRequest";
        service_vars["longrunning_set_operation_fields"] = R"""(
      r.set_project(request.project());
      r.set_region(request.region());
      r.set_operation(op);
)""";
        service_vars["longrunning_get_operation_path"] =
            R"""(absl::StrCat("/compute/v1/projects/", request.project(),
                             "/regions/", request.region(), "/operations/",
                             request.operation()))""";
        service_vars["longrunning_cancel_operation_path"] =
            R"""(absl::StrCat("/compute/v1/projects/", request.project(),
                             "/regions/", request.region(), "/operations/",
                             request.operation()))""";
      } else if (operation_service_extension == "ZoneOperations") {
        service_vars["longrunning_operation_include_header"] =
            "google/cloud/compute/zone_operations/v1/zone_operations.pb.h";
        service_vars["longrunning_get_operation_request_type"] =
            "google::cloud::cpp::compute::zone_operations::v1::"
            "GetZoneOperationRequest";
        service_vars["longrunning_cancel_operation_request_type"] =
            "google::cloud::cpp::compute::zone_operations::v1::"
            "DeleteZoneOperationRequest";
        service_vars["longrunning_set_operation_fields"] = R"""(
      r.set_project(request.project());
      r.set_zone(request.zone());
      r.set_operation(op);
)""";
        service_vars["longrunning_get_operation_path"] =
            R"""(absl::StrCat("/compute/v1/projects/", request.project(),
                             "/zones/", request.zone(), "/operations/",
                             request.operation()))""";
        service_vars["longrunning_cancel_operation_path"] =
            R"""(absl::StrCat("/compute/v1/projects/", request.project(),
                             "/zones/", request.zone(), "/operations/",
                             request.operation()))""";
      } else {
        std::cerr << __func__ << " Unknown operation_service_extension="
                  << operation_service_extension << std::endl;
        std::exit(1);
      }

      return;
    }
  }
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

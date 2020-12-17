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

#include "generator/internal/predicate_utils.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::FileDescriptorProto;

bool PredicateTrue(int const&) { return true; }
bool PredicateFalse(int const&) { return false; }

TEST(PredicateUtilsTest, GenericNot) {
  int const unused = 0;
  EXPECT_TRUE(GenericNot<int>(PredicateFalse)(unused));
  EXPECT_FALSE(GenericNot<int>(PredicateTrue)(unused));
}

TEST(PredicateUtilsTest, GenericAnd) {
  int const unused = 0;
  EXPECT_TRUE(GenericAnd<int>(PredicateTrue, PredicateTrue)(unused));
  EXPECT_FALSE(GenericAnd<int>(PredicateFalse, PredicateTrue)(unused));
  EXPECT_FALSE(GenericAnd<int>(PredicateTrue, PredicateFalse)(unused));
  EXPECT_FALSE(GenericAnd<int>(PredicateFalse, PredicateFalse)(unused));

  EXPECT_TRUE(
      GenericAnd<int>(PredicateTrue, GenericNot<int>(PredicateFalse))(unused));
  EXPECT_TRUE(
      GenericAnd<int>(GenericNot<int>(PredicateFalse), PredicateTrue)(unused));
  EXPECT_TRUE(
      GenericNot<int>(GenericAnd<int>(PredicateTrue, PredicateFalse))(unused));
}

TEST(PredicateUtilsTest, GenericOr) {
  int const unused = 0;
  EXPECT_TRUE(GenericOr<int>(PredicateTrue, PredicateTrue)(unused));
  EXPECT_TRUE(GenericOr<int>(PredicateFalse, PredicateTrue)(unused));
  EXPECT_TRUE(GenericOr<int>(PredicateTrue, PredicateFalse)(unused));
  EXPECT_FALSE(GenericOr<int>(PredicateFalse, PredicateFalse)(unused));
}

TEST(PredicateUtilsTest, GenericAll) {
  int const unused = 0;
  EXPECT_TRUE(GenericAll<int>(PredicateTrue)(unused));
  EXPECT_FALSE(GenericAll<int>(PredicateFalse)(unused));
  EXPECT_FALSE(GenericAll<int>(PredicateFalse, PredicateFalse)(unused));
  EXPECT_TRUE(GenericAll<int>(PredicateTrue, PredicateTrue)(unused));
  EXPECT_TRUE(
      GenericAll<int>(PredicateTrue, PredicateTrue, PredicateTrue)(unused));
  EXPECT_FALSE(
      GenericAll<int>(PredicateFalse, PredicateTrue, PredicateTrue)(unused));
  EXPECT_FALSE(
      GenericAll<int>(PredicateTrue, PredicateFalse, PredicateTrue)(unused));
  EXPECT_FALSE(
      GenericAll<int>(PredicateFalse, PredicateFalse, PredicateFalse)(unused));

  EXPECT_FALSE(GenericAll<int>(
      PredicateFalse, GenericOr<int>(PredicateFalse, PredicateTrue))(unused));
}

TEST(PredicateUtilsTest, GenericAny) {
  int const unused = 0;
  EXPECT_TRUE(GenericAny<int>(PredicateTrue)(unused));
  EXPECT_FALSE(GenericAny<int>(PredicateFalse)(unused));
  EXPECT_FALSE(GenericAny<int>(PredicateFalse, PredicateFalse)(unused));
  EXPECT_TRUE(GenericAny<int>(PredicateTrue, PredicateTrue)(unused));
  EXPECT_TRUE(
      GenericAny<int>(PredicateTrue, PredicateTrue, PredicateTrue)(unused));
  EXPECT_TRUE(
      GenericAny<int>(PredicateFalse, PredicateTrue, PredicateTrue)(unused));
  EXPECT_TRUE(
      GenericAny<int>(PredicateTrue, PredicateFalse, PredicateTrue)(unused));
  EXPECT_FALSE(
      GenericAny<int>(PredicateFalse, PredicateFalse, PredicateFalse)(unused));
}

TEST(PredicateUtilsTest, HasLongRunningMethodNone) {
  FileDescriptorProto longrunning_file;
  /// @cond
  auto constexpr kLongrunningText = R"pb(
    name: "google/longrunning/operations.proto"
    package: "google.longrunning"
    message_type { name: "Operation" }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kLongrunningText,
                                                            &longrunning_file));

  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    dependency: "google/longrunning/operations.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type { name: "Empty" }
    service {
      name: "Service"
      method {
        name: "One"
        input_type: "google.protobuf.Bar"
        output_type: "google.protobuf.Empty"
      }
      method {
        name: "Two"
        input_type: "google.protobuf.Bar"
        output_type: "google.protobuf.Bar"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  pool.BuildFile(longrunning_file);
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(HasLongrunningMethod(*service_file_descriptor->service(0)));
}

TEST(PredicateUtilsTest, HasLongRunningMethodOne) {
  FileDescriptorProto longrunning_file;
  /// @cond
  auto constexpr kLongrunningText = R"pb(
    name: "google/longrunning/operations.proto"
    package: "google.longrunning"
    message_type { name: "Operation" }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kLongrunningText,
                                                            &longrunning_file));
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    dependency: "google/longrunning/operations.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type { name: "Empty" }
    service {
      name: "Service"
      method {
        name: "One"
        input_type: "google.protobuf.Bar"
        output_type: "google.longrunning.Operation"
      }
      method {
        name: "Two"
        input_type: "google.protobuf.Bar"
        output_type: "google.protobuf.Bar"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  pool.BuildFile(longrunning_file);
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_TRUE(HasLongrunningMethod(*service_file_descriptor->service(0)));
}

TEST(PredicateUtilsTest, HasLongRunningMethodMoreThanOne) {
  FileDescriptorProto longrunning_file;
  /// @cond
  auto constexpr kLongrunningText = R"pb(
    name: "google/longrunning/operations.proto"
    package: "google.longrunning"
    message_type { name: "Operation" }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kLongrunningText,
                                                            &longrunning_file));
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    dependency: "google/longrunning/operations.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type { name: "Empty" }
    service {
      name: "Service"
      method {
        name: "One"
        input_type: "google.protobuf.Bar"
        output_type: "google.protobuf.Bar"
      }
      method {
        name: "Two"
        input_type: "google.protobuf.Bar"
        output_type: "google.longrunning.Operation"
      }
      method {
        name: "Three"
        input_type: "google.protobuf.Bar"
        output_type: "google.longrunning.Operation"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  pool.BuildFile(longrunning_file);
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_TRUE(HasLongrunningMethod(*service_file_descriptor->service(0)));
}

TEST(PredicateUtilsTest, IsResponseTypeEmpty) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type { name: "Empty" }
    service {
      name: "Service"
      method {
        name: "Empty"
        input_type: "google.protobuf.Bar"
        output_type: "google.protobuf.Empty"
      }
      method {
        name: "NonEmpty"
        input_type: "google.protobuf.Bar"
        output_type: "google.protobuf.Bar"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_TRUE(
      IsResponseTypeEmpty(*service_file_descriptor->service(0)->method(0)));
  EXPECT_FALSE(
      IsResponseTypeEmpty(*service_file_descriptor->service(0)->method(1)));
}

TEST(PredicateUtilsTest, IsLongrunningOperation) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.longrunning"
    message_type { name: "Bar" }
    message_type { name: "Operation" }
    service {
      name: "Service"
      method {
        name: "Lro"
        input_type: "google.longrunning.Bar"
        output_type: "google.longrunning.Operation"
      }
      method {
        name: "NonLro"
        input_type: "google.longrunning.Bar"
        output_type: "google.longrunning.Bar"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_TRUE(
      IsLongrunningOperation(*service_file_descriptor->service(0)->method(0)));
  EXPECT_FALSE(
      IsLongrunningOperation(*service_file_descriptor->service(0)->method(1)));
}

TEST(PredicateUtilsTest, IsNonStreaming) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Input" }
    message_type { name: "Output" }
    service {
      name: "Service"
      method {
        name: "NonStreaming"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
      method {
        name: "ClientStreaming"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
        client_streaming: true
      }
      method {
        name: "ServerStreaming"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
        server_streaming: true
      }
      method {
        name: "BidirectionalStreaming"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
        client_streaming: true
        server_streaming: true
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor_lro =
      pool.BuildFile(service_file);
  EXPECT_TRUE(
      IsNonStreaming(*service_file_descriptor_lro->service(0)->method(0)));
  EXPECT_FALSE(
      IsNonStreaming(*service_file_descriptor_lro->service(0)->method(1)));
  EXPECT_FALSE(
      IsNonStreaming(*service_file_descriptor_lro->service(0)->method(2)));
  EXPECT_FALSE(
      IsNonStreaming(*service_file_descriptor_lro->service(0)->method(3)));
}

TEST(PredicateUtilsTest, IsLongrunningMetadataTypeUsedAsResponseEmptyResponse) {
  FileDescriptorProto longrunning_file;
  auto constexpr kLongrunningText = R"pb(
    name: "google/longrunning/operation.proto"
    package: "google.longrunning"
    message_type { name: "Operation" }
  )pb";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kLongrunningText,
                                                            &longrunning_file));
  google::protobuf::FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    dependency: "google/longrunning/operation.proto"
    message_type { name: "Bar" }
    message_type { name: "Empty" }
    service {
      name: "Service"
      method {
        name: "Method0"
        input_type: "google.protobuf.Bar"
        output_type: "google.longrunning.Operation"
        options {
          [google.longrunning.operation_info] {
            response_type: "google.protobuf.Empty"
            metadata_type: "google.protobuf.Method2Metadata"
          }
          [google.api.http] {
            put: "/v1/{parent=projects/*/instances/*}/databases"
          }
        }
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  pool.BuildFile(longrunning_file);
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_TRUE(IsLongrunningMetadataTypeUsedAsResponse(
      *service_file_descriptor->service(0)->method(0)));
}

TEST(PredicateUtilsTest,
     IsLongrunningMetadataTypeUsedAsResponseNonEmptyResponse) {
  FileDescriptorProto longrunning_file;
  auto constexpr kLongrunningText = R"pb(
    name: "google/longrunning/operation.proto"
    package: "google.longrunning"
    message_type { name: "Operation" }
  )pb";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kLongrunningText,
                                                            &longrunning_file));
  google::protobuf::FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    dependency: "google/longrunning/operation.proto"
    message_type { name: "Bar" }
    message_type { name: "Empty" }
    service {
      name: "Service"
      method {
        name: "Method0"
        input_type: "google.protobuf.Bar"
        output_type: "google.longrunning.Operation"
        options {
          [google.longrunning.operation_info] {
            response_type: "google.protobuf.Method2Response"
            metadata_type: "google.protobuf.Method2Metadata"
          }
          [google.api.http] {
            patch: "/v1/{parent=projects/*/instances/*}/databases"
          }
        }
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  pool.BuildFile(longrunning_file);
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsLongrunningMetadataTypeUsedAsResponse(
      *service_file_descriptor->service(0)->method(0)));
}

TEST(PredicateUtilsTest,
     IsLongrunningMetadataTypeUsedAsResponseNotLongrunning) {
  google::protobuf::FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type { name: "Empty" }
    service {
      name: "Service"
      method {
        name: "Method0"
        input_type: "google.protobuf.Bar"
        output_type: "google.protobuf.Empty"
        options {
          [google.api.http] {
            patch: "/v1/{parent=projects/*/instances/*}/databases"
          }
        }
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsLongrunningMetadataTypeUsedAsResponse(
      *service_file_descriptor->service(0)->method(0)));
}

TEST(PredicateUtilsTest, PaginationSuccess) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "page_size" number: 1 type: TYPE_INT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "repeated_field"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
    service {
      name: "Service"
      method {
        name: "Paginated"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_TRUE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
  auto result =
      DeterminePagination(*service_file_descriptor->service(0)->method(0));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->first, "repeated_field");
  EXPECT_EQ(result->second, "google.protobuf.Bar");
}

TEST(PredicateUtilsTest, PaginationNoPageSize) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type { name: "Input" }
    message_type { name: "Output" }
    service {
      name: "Service"
      method {
        name: "NoPageSize"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
}

TEST(PredicateUtilsTest, PaginationNoPageToken) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "page_size" number: 1 type: TYPE_INT32 }
    }
    message_type { name: "Output" }
    service {
      name: "Service"
      method {
        name: "NoPageToken"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
}

TEST(PredicateUtilsTest, PaginationNoNextPageToken) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "page_size" number: 1 type: TYPE_INT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type { name: "Output" }
    service {
      name: "Service"
      method {
        name: "NoNextPageToken"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
}

TEST(PredicateUtilsTest, PaginationNoRepeatedMessageField) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "page_size" number: 1 type: TYPE_INT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "repeated_field"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_INT32
      }
    }
    service {
      name: "Service"
      method {
        name: "NoRepeatedMessage"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
}

TEST(PredicateUtilsDeathTest, PaginationRepeatedMessageOrderMismatch) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type { name: "Foo" }
    message_type {
      name: "Input"
      field { name: "page_size" number: 1 type: TYPE_INT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "repeated_message_field_first"
        number: 3
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Foo"
      }
      field {
        name: "repeated_message_field_second"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
    service {
      name: "Service"
      method {
        name: "RepeatedOrderMismatch"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_DEATH(IsPaginated(*service_file_descriptor->service(0)->method(0)),
               "");
}

TEST(PredicateUtilsTest, HasPaginatedMethodTrue) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "page_size" number: 1 type: TYPE_INT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "repeated_field"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
    service {
      name: "Service"
      method {
        name: "Paginated"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_TRUE(HasPaginatedMethod(*service_file_descriptor->service(0)));
}

TEST(PredicateUtilsTest, HasPaginatedMethodFalse) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type { name: "Input" }
    message_type { name: "Output" }
    service {
      name: "Service"
      method {
        name: "NoPageSize"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(HasPaginatedMethod(*service_file_descriptor->service(0)));
}

TEST(PredicateUtilsTest, PredicatedFragmentTrueString) {
  int const unused = 0;
  PredicatedFragment<int> f = {PredicateTrue, "True", "False"};
  EXPECT_EQ(f(unused), "True");
}

TEST(PredicateUtilsTest, PredicatedFragmentFalseString) {
  int const unused = 0;
  PredicatedFragment<int> f = {PredicateFalse, "True", "False"};
  EXPECT_EQ(f(unused), "False");
}

TEST(PredicateUtilsTest, PredicatedFragmentStringOnly) {
  int const unused = 0;
  PredicatedFragment<int> f = {"True"};
  EXPECT_EQ(f(unused), "True");
}

TEST(Pattern, OperatorParens) {
  int const unused = 0;
  Pattern<int> p({}, PredicateFalse);
  EXPECT_FALSE(p(unused));
}

TEST(Pattern, FragmentsAccessor) {
  int const unused = 0;
  Pattern<int> p({{PredicateFalse, "fragment0_true", "fragment0_false"},
                  {PredicateTrue, "fragment1_true", "fragment1_false"}},
                 PredicateTrue);
  EXPECT_TRUE(p(unused));
  std::string result;
  for (auto const& pf : p.fragments()) {
    result += pf(unused);
  }

  EXPECT_EQ(result, std::string("fragment0_falsefragment1_true"));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

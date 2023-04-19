// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "generator/internal/pagination.h"
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

TEST(PaginationTest, PaginationAIP4233Success) {
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
  EXPECT_EQ(result->second->full_name(), "google.protobuf.Bar");
}

TEST(PaginationTest, PaginationAIP4233NoPageSize) {
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

TEST(PaginationTest, PaginationAIP4233NoPageToken) {
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

TEST(PaginationTest, PaginationAIP4233NoNextPageToken) {
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

TEST(PaginationTest, PaginationAIP4233NoRepeatedMessageField) {
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

TEST(PredicateUtilsDeathTest, PaginationAIP4233RepeatedMessageOrderMismatch) {
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
  EXPECT_DEATH_IF_SUPPORTED(
      IsPaginated(*service_file_descriptor->service(0)->method(0)),
      "Repeated field in paginated response must be first");
}

TEST(PaginationTest, PaginationAIP4233ExactlyOneRepatedStringResponse) {
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
        type: TYPE_STRING
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
  EXPECT_EQ(result->second, nullptr);
}

TEST(PaginationTest, PaginationRestSuccess) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "max_results" number: 1 type: TYPE_UINT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "items"
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
  EXPECT_EQ(result->first, "items");
  EXPECT_EQ(result->second->full_name(), "google.protobuf.Bar");
}

TEST(PaginationTest, PaginationRestNoMaxResults) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "items"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
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

TEST(PaginationTest, PaginationRestMaxResultsWrongType) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "max_results" number: 1 type: TYPE_INT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "items"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
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

TEST(PaginationTest, PaginationRestNoPageToken) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "max_results" number: 1 type: TYPE_UINT32 }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "items"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
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

TEST(PaginationTest, PaginationRestNoNextPageToken) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "max_results" number: 1 type: TYPE_UINT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field {
        name: "items"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
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

TEST(PaginationTest, PaginationRestNoItems) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "max_results" number: 1 type: TYPE_UINT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "bars"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
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

TEST(PaginationTest, PaginationRestItemsNotRepeated) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "max_results" number: 1 type: TYPE_UINT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "items"
        number: 2
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
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

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

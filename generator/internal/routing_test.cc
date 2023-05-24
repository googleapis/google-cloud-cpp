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

#include "generator/internal/routing.h"
#include "generator/testing/error_collectors.h"
#include "generator/testing/fake_source_tree.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::generator_testing::FakeSourceTree;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::FileDescriptorProto;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::UnorderedElementsAre;

// We factor out the protobuf mumbo jumbo to keep the tests concise and
// self-contained. The protobuf objects must be in scope for the duration of a
// test. To achieve this, we pass in a `test` lambda which is invoked in this
// method.
void RunRoutingTest(std::string const& service_proto,
                    std::function<void(FileDescriptor const*)> const& test) {
  auto constexpr kServiceBoilerPlate = R"""(
syntax = "proto3";
package google.protobuf;
import "google/api/routing.proto";
)""";

  auto constexpr kRoutingProto = R"""(
syntax = "proto3";
package google.api;
import "google/protobuf/descriptor.proto";

extend google.protobuf.MethodOptions {
  google.api.RoutingRule routing = 72295729;
}
message RoutingRule {
  repeated RoutingParameter routing_parameters = 2;
}
message RoutingParameter {
  string field = 1;
  string path_template = 2;
}
)""";

  FakeSourceTree source_tree(std::map<std::string, std::string>{
      {"google/api/routing.proto", kRoutingProto},
      {"google/cloud/foo/service.proto", kServiceBoilerPlate + service_proto}});
  google::protobuf::compiler::SourceTreeDescriptorDatabase source_tree_db(
      &source_tree);
  google::protobuf::SimpleDescriptorDatabase simple_db;
  FileDescriptorProto file_proto;
  // we need descriptor.proto to be accessible by the pool
  // since our test file imports it
  FileDescriptorProto::descriptor()->file()->CopyTo(&file_proto);
  simple_db.Add(file_proto);
  google::protobuf::MergedDescriptorDatabase merged_db(&simple_db,
                                                       &source_tree_db);
  generator_testing::ErrorCollector collector;
  DescriptorPool pool(&merged_db, &collector);

  // Run the test
  test(pool.FindFileByName("google/cloud/foo/service.proto"));
}

Matcher<RoutingParameter const&> RP(std::string const& field_name,
                                    std::string const& pattern) {
  return AllOf(Field(&RoutingParameter::field_name, field_name),
               Field(&RoutingParameter::pattern, pattern));
}

TEST(ParseExplicitRoutingHeaderTest, NoRouting) {
  auto constexpr kProto = R"""(
message Foo {
  string foo = 1;
  string bar = 2;
}

service Service0 {
  rpc Method0(Foo) returns (Foo) {}
}
)""";

  RunRoutingTest(kProto, [](FileDescriptor const* fd) {
    auto const& method = *fd->service(0)->method(0);
    auto info = ParseExplicitRoutingHeader(method);
    EXPECT_THAT(info, UnorderedElementsAre());
  });
}

TEST(ParseExplicitRoutingHeaderTest, Regex) {
  auto constexpr kProto = R"""(
message Foo {
  string foo = 1;
  string bar = 2;
}

service Service0 {
  rpc Method0(Foo) returns (Foo) {
    option (google.api.routing) = {
      routing_parameters {
        field: "foo"
        path_template: "projects/*/{routing_key=instances/*}/**"
      }
      routing_parameters {
        field: "foo"
        path_template: "projects/*:{handles_colon=instances/*}:**"
      }
    };
  }
}
)""";

  RunRoutingTest(kProto, [](FileDescriptor const* fd) {
    auto const& method = *fd->service(0)->method(0);
    auto info = ParseExplicitRoutingHeader(method);
    EXPECT_THAT(
        info,
        UnorderedElementsAre(
            Pair("routing_key",
                 ElementsAre(RP("foo", "projects/[^/]+/(instances/[^/]+)/.*"))),
            Pair("handles_colon",
                 ElementsAre(
                     RP("foo", "projects/[^:]+:(instances/[^:]+):.*")))));
  });
}

TEST(ParseExplicitRoutingHeaderTest, NoPathTemplate) {
  auto constexpr kProto = R"""(
message Foo {
  string foo = 1;
  string bar = 2;
}

service Service0 {
  rpc Method0(Foo) returns (Foo) {
    option (google.api.routing) = {
      routing_parameters {
        field: "foo"
      }
    };
  }
}
)""";

  RunRoutingTest(kProto, [](FileDescriptor const* fd) {
    auto const& method = *fd->service(0)->method(0);
    auto info = ParseExplicitRoutingHeader(method);
    // When the path template is not present, we should use the field name as
    // the routing parameter key, and our regex should have one capture group
    // that matches all.
    EXPECT_THAT(info, UnorderedElementsAre(
                          Pair("foo", ElementsAre(RP("foo", "(.*)")))));
  });
}

TEST(ParseExplicitRoutingHeaderTest, OrderReversed) {
  auto constexpr kProto = R"""(
message Foo {
  string foo = 1;
  string bar = 2;
}

service Service0 {
  rpc Method0(Foo) returns (Foo) {
    option (google.api.routing) = {
      routing_parameters {
        field: "foo"
        path_template: "{routing_key=foo-path-1}"
      }
      routing_parameters {
        field: "bar"
        path_template: "{routing_key=bar-path-2}"
      }
      routing_parameters {
        field: "foo"
        path_template: "{routing_key=foo-path-3}"
      }
    };
  }
}
)""";

  RunRoutingTest(kProto, [](FileDescriptor const* fd) {
    auto const& method = *fd->service(0)->method(0);
    auto info = ParseExplicitRoutingHeader(method);
    EXPECT_THAT(
        info, UnorderedElementsAre(
                  Pair("routing_key", ElementsAre(RP("foo", "(foo-path-3)"),
                                                  RP("bar", "(bar-path-2)"),
                                                  RP("foo", "(foo-path-1)")))));
  });
}

TEST(ParseExplicitRoutingHeaderTest, GroupsByRoutingKey) {
  auto constexpr kProto = R"""(
message Foo {
  string foo = 1;
  string bar = 2;
}

service Service0 {
  rpc Method0(Foo) returns (Foo) {
    option (google.api.routing) = {
      routing_parameters {
        field: "foo"
        path_template: "{routing_key1=foo-path-1}"
      }
      routing_parameters {
        field: "foo"
        path_template: "{routing_key2=foo-path-2}"
      }
      routing_parameters {
        field: "bar"
        path_template: "{routing_key1=bar-path-3}"
      }
      routing_parameters {
        field: "bar"
        path_template: "{routing_key2=bar-path-4}"
      }
    };
  }
}
)""";

  RunRoutingTest(kProto, [](FileDescriptor const* fd) {
    auto const& method = *fd->service(0)->method(0);
    auto info = ParseExplicitRoutingHeader(method);
    EXPECT_THAT(
        info,
        UnorderedElementsAre(
            Pair("routing_key1", ElementsAre(RP("bar", "(bar-path-3)"),
                                             RP("foo", "(foo-path-1)"))),
            Pair("routing_key2", ElementsAre(RP("bar", "(bar-path-4)"),
                                             RP("foo", "(foo-path-2)")))));
  });
}

TEST(ParseExplicitRoutingHeaderTest, HandlesFieldNameConflict) {
  auto constexpr kProto = R"""(
message Foo {
  string namespace = 1;
}

service Service0 {
  rpc Method0(Foo) returns (Foo) {
    option (google.api.routing) = {
      routing_parameters {
        field: "namespace"
      }
    };
  }
}
)""";

  RunRoutingTest(kProto, [](FileDescriptor const* fd) {
    auto const& method = *fd->service(0)->method(0);
    auto info = ParseExplicitRoutingHeader(method);
    // Note that while the field name has been modified so that it does not
    // conflict with the C++ keyword, the routing key must not change.
    EXPECT_THAT(info, UnorderedElementsAre(Pair(
                          "namespace", ElementsAre(RP("namespace_", "(.*)")))));
  });
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_TESTING_DESCRIPTOR_POOL_FIXTURE_H
#define GOOGLE_CLOUD_CPP_GENERATOR_TESTING_DESCRIPTOR_POOL_FIXTURE_H

#include "generator/testing/fake_source_tree.h"
#include "google/protobuf/compiler/importer.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_testing {

/**
 * Implements a fixture for testing with a `google::protobuf::DescriptorPool`.
 *
 * Some tests need a properly initialized descriptor pool, with:
 * - the basic protobuf types and options already available
 * - error collectors so the test fails with meaningful errors if there is a
 *   test bug
 * - basic Google features, like longrunning operations.
 *
 * This class provides all these features so we don't duplicate them across
 * tests.  It also provides helpers to add more (simulated) `.proto` files.
 */
class DescriptorPoolFixture : public ::testing::Test {
 public:
  DescriptorPoolFixture();

  /**
   * Returns the descriptor for a given file.
   *
   * Implicitly, this "compiles" the file and validates it. It can be used
   * to verify the imports compile correctly before adding some other test
   * proto contents.
   */
  google::protobuf::FileDescriptor const* FindFile(
      std::string const& name) const;

  /**
   * Adds a new proto file and "compiles" it.
   *
   * Typically used to set up the conditions of a test, as in:
   *
   * @code
   * class MyTest : public DescriptorPoolFixture {};
   *
   * TEST_F(MyTest, Name) {
   *   ASSERT_TRUE(AddProtoFile("foo.proto", contents));
   * }
   * @endcode
   */
  bool AddProtoFile(std::string const& name, std::string contents);

  google::protobuf::DescriptorPool const& pool() const { return pool_; }

 private:
  std::unique_ptr<google::protobuf::DescriptorPool::ErrorCollector>
      descriptor_error_collector_;
  std::unique_ptr<google::protobuf::compiler::MultiFileErrorCollector>
      multifile_error_collector_;
  FakeSourceTree source_tree_;
  google::protobuf::SimpleDescriptorDatabase simple_db_;
  google::protobuf::compiler::SourceTreeDescriptorDatabase source_tree_db_;
  google::protobuf::MergedDescriptorDatabase merged_db_;
  google::protobuf::DescriptorPool pool_;
};

}  // namespace generator_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_TESTING_DESCRIPTOR_POOL_FIXTURE_H

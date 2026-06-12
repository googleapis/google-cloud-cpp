#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/mock_client.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

TEST(BzlmodTest, CanCreateMockClient) {
  // Create a mock client.
  auto mock = std::make_shared<google::cloud::storage::testing::MockClient>();

  // Use the mock to construct a Client object. This verifies that the
  // necessary symbols are linked correctly.
  google::cloud::storage::Client client(mock);

  // A simple assertion to make it a valid test.
  ASSERT_TRUE(true);
}
